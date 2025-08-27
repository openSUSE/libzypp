#include <string>
#include <stdio.h>
#include <poll.h>
#include <signal.h>

#include <zypp-core/base/Logger.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/Exception.h>
#include <zypp-core/ExternalProgram.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/ng/base/private/linuxhelpers_p.h>

#include <tests/lib/FtpServer.h>
#include <tests/lib/TestTools.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>

#ifndef TESTS_SHARED_DIR
#error "TESTS_SHARED_DIR not defined"
#endif

#ifndef FTPSRV_BINARY
#error "FTPSRV_BINARY not defined"
#endif

using std::endl;
using namespace zypp;

namespace  {

  /** ExternalProgram extended to kill the child process if the parent thread exits
     */
  class ZYPP_LOCAL ExternalTrackedProgram : public ExternalProgram
  {
  public:
    ExternalTrackedProgram (const char *const *argv, const Environment & environment,
      Stderr_Disposition stderr_disp = Normal_Stderr,
      bool use_pty = false, int stderr_fd = -1, bool default_locale = false,
      const Pathname& root = "") : ExternalProgram()
    {
      start_program( argv, environment, stderr_disp, stderr_fd, default_locale, root.c_str(), false, true );
    }

  };
}

class FtpServer::Impl
{
public:
    Impl(const Pathname &root, unsigned int port, bool ssl)
      : _docroot(root), _port(port), _stop(false), _stopped(true), _ssl( ssl )
    {
      // wake up pipe to interrupt poll()
      pipe ( _wakeupPipe );
      fcntl( _wakeupPipe[0], F_SETFL, O_NONBLOCK );

      MIL << "Working dir is " << _workingDir << endl;

      filesystem::assert_dir( _workingDir / "log" );
      filesystem::assert_dir( _workingDir / "tmp" );
    }

    ~Impl()
    {
        if ( ! _stopped )
            stop();

        close (_wakeupPipe[0]);
        close (_wakeupPipe[1]);
    }

    int port() const
    {
        return _port;
    }

    void worker_thread()
    {

      /*
       * -o vsftpd_log_file=/var/log/vsftpd.log
       *
       * ssl_enable=NO
       * rsa_cert_file=
       * dsa_cert_file=
       * listen_port
       *
       * -o anon_root=/tmp/ftptest
       * -o local_root=/tmp/ftptest
       *
       */

      ExternalProgram::Environment env;

      const auto confPath = _workingDir.path();
      const auto confFile = confPath/"vsftpd.conf";
      bool canContinue = true;

      {
        std::lock_guard<std::mutex> lock ( _mut );
        canContinue = ( zypp::filesystem::symlink( Pathname(TESTS_SHARED_DIR)/"data"/"vsftpconf"/"vsftpd.conf",  confFile ) == 0 );
        if ( canContinue && _ssl ) canContinue = zypp::filesystem::symlink( Pathname(TESTS_SHARED_DIR)/"data"/"webconf"/"ssl"/"server.pem",  confPath/"cert.pem") == 0;
        if ( canContinue && _ssl ) canContinue = zypp::filesystem::symlink( Pathname(TESTS_SHARED_DIR)/"data"/"webconf"/"ssl"/"server.key",  confPath/"cert.key") == 0;
      }

      const std::string logfile    = (zypp::str::Format("-vsftpd_log_file=%1%")   % (confPath/"logfile")).asString();
      const std::string anonRoot   = (zypp::str::Format("-oanon_root=%1%")   % _docroot).asString();
      const std::string localRoot  = (zypp::str::Format("-olocal_root=%1%")  % _docroot).asString();
      const std::string listenPort = (zypp::str::Format("-olisten_port=%1%") % _port).asString();

      const std::string pemFile    = (zypp::str::Format("-orsa_cert_file=%1%") %  (confPath/"cert.pem") ).asString ();
      const std::string keyFile    = (zypp::str::Format("-orsa_private_key_file=%1%") %  (confPath/"cert.key") ).asString ();

      std::vector<const char *> argv = {
        FTPSRV_BINARY,
        confFile.c_str(),
        anonRoot.c_str(),
        localRoot.c_str(),
        listenPort.c_str(),
      };

      if ( _ssl ) {
        argv.push_back ( "-ossl_enable=YES" );
        argv.push_back ( "-oallow_anon_ssl=YES" );
        argv.push_back ( pemFile.c_str() );
        argv.push_back ( keyFile.c_str() );
      }

      argv.push_back(nullptr);


      if ( !canContinue ) {
        _stopped = true;
        _cv.notify_all();
        return;
      }

        ExternalTrackedProgram prog( argv.data(), env, ExternalProgram::Stderr_To_Stdout, false, -1, true);
        prog.setBlocking( false );

        while(1) {
          std::string line = prog.receiveLine();
          if ( line.empty() )
            break;
          std::cerr << line << endl;
        };

        // Wait max 10 sec for the socket becoming available
        bool isup { TestTools::checkLocalPort( port() ) };
        if ( !isup )
        {
          unsigned i = 0;
          do {
            std::this_thread::sleep_for( std::chrono::milliseconds(1000) );
            isup = TestTools::checkLocalPort( port() );
          } while ( !isup && ++i < 10 );

          if ( !isup && prog.running() ) {
            prog.kill( SIGTERM );
            prog.close();
          }
        }

        if ( !prog.running() ) {
          _stop = true;
          _stopped = true;
        } else {
          _stopped = false;
        }

        struct pollfd fds[] { {
            _wakeupPipe[0],
            POLLIN,
            0
          }, {
            fileno( prog.inputFile() ),
            POLLIN | POLLHUP | POLLERR,
            0
          }
        };

        bool firstLoop = true;

        while ( 1 ) {

          if ( firstLoop ) {
            firstLoop = false;
            _cv.notify_all();
            if ( _stopped )
              return;
          }

          //listen on a pipe for a wakeup event to stop the worker if required
          int ret = zyppng::eintrSafeCall( ::poll, fds, 2, -1 );

          if ( ret < 0 ) {
            std::cerr << "Poll error " << errno << endl;
            continue;
          }

          if ( fds[0].revents ) {
            //clear pipe
            char dummy;
            while ( read( _wakeupPipe[0], &dummy, 1 ) > 0 ) { continue; }
          }

          if ( fds[1].revents ) {
            while(1) {
              std::string line = prog.receiveLine();
              if ( line.empty() )
                break;
              std::cerr << line << endl;
            };

            if ( !prog.running() ) {
              std::cerr << "Ftpserver exited too early" << endl;
              _stop = true;
              _stopped = true;
            }
          }

          if ( _stop )
            break;
        }

        if ( prog.running() ) {
            prog.kill( SIGTERM );
            prog.close();
        }
    }

    std::string log() const
    {
      //read logfile
      return std::string();
    }

    void stop()
    {
        MIL << "Waiting for server thread to finish" << endl;
        _stop = true;

        //signal the thread to wake up
        write( _wakeupPipe[1], "\n", 1);

        _thrd->join();
        MIL << "server thread finished" << endl;

        _thrd.reset();
        _stopped = true;
    }

    void start()
    {
      if ( !filesystem::PathInfo( _docroot ).isExist() ) {
        std::cerr << "Invalid docroot" << std::endl;
        throw zypp::Exception("Invalid docroot");
      }

      if ( !_stopped ) {
        stop();
      }

      _stop = _stopped = false;

      std::unique_lock<std::mutex> lock( _mut );
      _thrd.reset( new std::thread( &Impl::worker_thread, this ) );
      _cv.wait(lock);

      if ( _stopped ) {
        _thrd->join();
        throw zypp::Exception("Failed to start the ftpserver");
      }
    }

    std::mutex _mut; //<one mutex to rule em all
    std::condition_variable _cv; //< to sync server startup

    filesystem::TmpDir _workingDir;
    zypp::Pathname _docroot;
    zypp::shared_ptr<std::thread> _thrd;

    unsigned int _port;
    int _wakeupPipe[2];

    std::atomic_bool _stop;
    bool _stopped;
    bool _ssl;
};


FtpServer::FtpServer(const Pathname &root, unsigned int port, bool useSSL)
    : _pimpl(new Impl(root, port, useSSL))
{
}

bool FtpServer::start()
{
    _pimpl->start();
    return !isStopped();
}


std::string FtpServer::log() const
{
  return _pimpl->log();
}

bool FtpServer::isStopped() const
{
  return _pimpl->_stopped;
}

int FtpServer::port() const
{
    return _pimpl->port();
}


Url FtpServer::url() const
{
    Url url;
    url.setHost("localhost");
    url.setPort(str::numstring(port()));
    url.setScheme("ftp");
    return url;
}

media::TransferSettings FtpServer::transferSettings() const
{
  media::TransferSettings set;
  set.setCertificateAuthoritiesPath(zypp::Pathname(TESTS_SHARED_DIR)/"data/webconf/ssl/certstore");
  return set;
}

void FtpServer::stop()
{
    _pimpl->stop();
}

FtpServer::~FtpServer()
{
}
