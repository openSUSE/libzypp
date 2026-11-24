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

#include <tests/lib/ProxyServer.h>
#include <tests/lib/TestTools.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>

#ifndef TESTS_SHARED_DIR
#error "TESTS_SHARED_DIR not defined"
#endif

#ifndef PROXYSRV_BINARY
#error "PROXYSRV_BINARY not defined"
#endif

#ifndef PROXYAUTH_BINARY
#error "PROXYAUTH_BINARY not defined"
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

class ProxyServer::Impl
{
public:
    Impl( unsigned int port )
      : _port(port)
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

      ExternalProgram::Environment env;

      const auto confPath = _workingDir.path();
      const auto confFile = confPath/"proxy.conf";
      const auto varConfFile = confPath/"testproxy.conf";
      bool canContinue = true;

      {
        std::lock_guard<std::mutex> lock ( _mut );
        std::string confTemplate =
          "http_port %1%\n"
          "\n"
          "# Leave coredumps in the first cache dir\n"
          "coredump_dir %2%\n"
          "\n"
          "pid_filename %2%/squid.pid\n"
          "access_log %2%/access.log\n"
          "cache_store_log %2%/cache-store.log\n"
          "cache_log %2%/cache.log\n"
        ;

        if ( _auth ) {
          confTemplate +=
              "auth_param basic program %3% %4%/htaccess\n"
              "acl foo proxy_auth REQUIRED\n"
              "http_access deny !foo\n"
          ;
        }

        canContinue = ( zypp::filesystem::symlink( Pathname(TESTS_SHARED_DIR)/"data"/"proxyconf"/"squid.conf",  confFile ) == 0 );
        if ( canContinue ) canContinue = TestTools::writeFile( varConfFile, str::Format(confTemplate) % _port % confPath % Pathname(PROXYAUTH_BINARY) % (Pathname(TESTS_SHARED_DIR)/"data"/"proxyconf") );
      }

      const std::string wDir( str::Format("#%1%") % confPath );

      std::vector<const char *> argv = {
        wDir.c_str(),
        PROXYSRV_BINARY,
        "--foreground",
        "-f", confFile.c_str()
      };

      argv.push_back(nullptr);


      // collects all information we have about a failed start, logs it and reports it to \ref start
      const auto &failStart = [&]( const std::string &reason, const std::string &output ) {
        str::Str msg;
        msg << "Failed to start the proxy server " << PROXYSRV_BINARY << " on port " << port() << ": " << reason;
        if ( !output.empty() )
          msg << "\n--- proxy output ---\n" << output;

        // squid reports most startup errors (FATAL: ...) only in its cache log
        std::string cacheLog = TestTools::readFile( confPath/"cache.log" );
        if ( !cacheLog.empty() ) {
          constexpr std::string::size_type maxLogSize = 4096;
          if ( cacheLog.size() > maxLogSize ) {
            cacheLog.erase( 0, cacheLog.size() - maxLogSize );
            cacheLog.erase( 0, cacheLog.find( '\n' ) + 1 );
            cacheLog.insert( 0, "[...]\n" );
          }
          msg << "\n--- " << confPath/"cache.log" << " ---\n" << cacheLog;
        }

        std::cerr << msg.str() << endl;
        ERR << msg.str() << endl;
        _stop = true;
        _stopped = true;
        notifyStarted( msg.str() );
      };

      if ( !canContinue ) {
        failStart( str::Str() << "Unable to write the proxy configuration to " << confPath, std::string() );
        return;
      }

        ExternalTrackedProgram prog( argv.data(), env, ExternalProgram::Stderr_To_Stdout, false, -1, true);
        prog.setBlocking( false );

        // remember everything the proxy prints during startup, we need it if the start fails
        std::string startupOutput;
        // read all available output, optionally appending it to \a collect_r. Since the process is
        // non blocking a empty line means there is nothing to read. The last piece might be an
        // incomplete line, the rest of it is returned by the next read.
        const auto &readOutput = [&]( std::string *collect_r ) {
          if ( !prog.inputFile() )
            return;
          while(1) {
            // a read on the empty non blocking pipe fails with EAGAIN and sets the error flag
            // of the stream, glibc refuses all further reads until it is cleared
            clearerr( prog.inputFile() );
            std::string line = prog.receiveLine();
            if ( line.empty() )
              break;
            std::cerr << line << endl;
            if ( collect_r )
              *collect_r += line;
          };
        };

        readOutput( &startupOutput );

        // Wait max 10 sec for the socket becoming available
        bool isup { TestTools::checkLocalPort( port() ) };
        if ( !isup )
        {
          unsigned i = 0;
          do {
            std::this_thread::sleep_for( std::chrono::milliseconds(1000) );
            readOutput( &startupOutput );
            isup = TestTools::checkLocalPort( port() );
          } while ( !isup && prog.running() && ++i < 10 );
        }

        readOutput( &startupOutput );

        if ( !prog.running() ) {
          // the proxy exited by itself
          const int status = prog.close();
          failStart( str::Str() << "The proxy exited with status " << status
                                << ( prog.execError().empty() ? std::string() : ( ": " + prog.execError() ) ), startupOutput );
          return;
        }

        if ( !isup ) {
          // the proxy is running, but does not listen on the port
          prog.kill( SIGTERM );
          prog.close();
          failStart( str::Str() << "The proxy did not open port " << port() << " within 10 seconds and was terminated", startupOutput );
          return;
        }
        _stopped = false;

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
            notifyStarted();
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
            readOutput( nullptr );

            if ( !prog.running() ) {
              std::cerr << "Proxy server exited too early" << endl;
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

    /** Signal \ref start that the worker thread finished its startup, successful or not.
     *  \a error_r describes why the start failed, empty on success.
     */
    void notifyStarted( std::string error_r = std::string() )
    {
      {
        std::lock_guard<std::mutex> lock( _mut );
        _started = true;
        _startError = std::move( error_r );
      }
      _cv.notify_all();
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

      if ( !_stopped ) {
        stop();
      }

      _stop = _stopped = false;

      std::unique_lock<std::mutex> lock( _mut );
      _started = false;
      _startError.clear();
      _thrd.reset( new std::thread( &Impl::worker_thread, this ) );
      _cv.wait( lock, [this]{ return _started; } );

      if ( _stopped ) {
        std::string err = _startError.empty() ? std::string("Failed to start the proxyserver") : _startError;
        lock.unlock();
        _thrd->join();
        throw zypp::Exception( err );
      }
    }

    std::mutex _mut; //<one mutex to rule em all
    std::condition_variable _cv; //< to sync server startup

    filesystem::TmpDir _workingDir;
    zypp::Pathname _docroot;
    zypp::shared_ptr<std::thread> _thrd;

    unsigned int _port;
    int _wakeupPipe[2];

    std::atomic_bool _stop = false;
    std::atomic_bool _stopped = true;
    bool _started = false; //< set by the worker thread once startup is done, guarded by _mut
    std::string _startError; //< why the last start failed, guarded by _mut
    bool _auth = false;
};


ProxyServer::ProxyServer( unsigned int port )
    : _pimpl(new Impl( port ))
{
}

bool ProxyServer::start()
{
    _pimpl->start();
    return !isStopped();
}


std::string ProxyServer::log() const
{
  return _pimpl->log();
}

bool ProxyServer::isStopped() const
{
  return _pimpl->_stopped;
}

void ProxyServer::setAuthEnabled(bool set)
{
  _pimpl->_auth = true;
}

int ProxyServer::port() const
{
    return _pimpl->port();
}

Url ProxyServer::url() const
{
    Url url;
    url.setHost("localhost");
    url.setPort(str::numstring(port()));
    url.setScheme("http");
    return url;
}

void ProxyServer::stop()
{
    _pimpl->stop();
}

ProxyServer::~ProxyServer()
{
}
