#include <sstream>
#include <string>
#include <stdio.h>
#include <poll.h>

#include "zypp/base/Logger.h"
#include "zypp/base/String.h"
#include "zypp/base/Exception.h"
#include "zypp/ExternalProgram.h"
#include "zypp/TmpPath.h"
#include "zypp/PathInfo.h"
#include "WebServer.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <fastcgi/fcgiapp.h>
#include <fastcgi/fcgio.h>

#ifndef TESTS_SRC_DIR
#error "TESTS_SRC_DIR not defined"
#endif

using namespace zypp;
using namespace std;

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

static inline string hostname()
{
    static char buf[256];
    string result;
    if (!::gethostname(buf, 255))
        result += string(buf);
    else
        return "localhost";
    return result;
}

static inline std::string handlerPrefix()
{
  static std::string pref("/handler/");
  return pref;
}

WebServer::Request::Request( std::istream::__streambuf_type *rinbuf, std::ostream::__streambuf_type *routbuf, std::ostream::__streambuf_type *rerrbuf )
  : rin  ( rinbuf )
  , rout ( routbuf )
  , rerr ( rerrbuf )
{ }

class WebServer::Impl
{
public:
    Impl(const Pathname &root, unsigned int port, bool ssl)
      : _docroot(root), _port(port), _stop(false), _stopped(true), _ssl( ssl )
    {
      FCGX_Init();

      // wake up pipe to interrupt poll()
      pipe ( _wakeupPipe );
      fcntl( _wakeupPipe[0], F_SETFL, O_NONBLOCK );

      MIL << "Working dir is " << _workingDir << endl;

      filesystem::assert_dir( _workingDir / "log" );
      filesystem::assert_dir( _workingDir / "state" );
      filesystem::assert_dir( _workingDir / "home" );
      filesystem::assert_dir( _workingDir / "cache" );
      filesystem::assert_dir( _workingDir / "upload" );
      filesystem::assert_dir( _workingDir / "sockets" );
    }

    ~Impl()
    {
        if ( ! _stopped )
            stop();

        close (_wakeupPipe[0]);
        close (_wakeupPipe[1]);
    }

    Pathname socketPath () const {
      return ( _workingDir.path() / "fcgi.sock" );
    }

    int port() const
    {
        return _port;
    }

    void worker_thread()
    {
      int sockFD = -1;

      ExternalProgram::Environment env;

      Pathname confDir = Pathname(TESTS_SRC_DIR) / "data" / "lighttpdconf";
      env["ZYPP_TEST_SRVCONF"] = confDir.asString();

      std::string confFile    = ( confDir / "lighttpd.conf" ).asString();

      {
        std::lock_guard<std::mutex> lock ( _mut );
        sockFD = FCGX_OpenSocket( socketPath().c_str(), 128 );
        env["ZYPP_TEST_SRVROOT"] = _workingDir.path().c_str();
        env["ZYPP_TEST_PORT"] = str::numstring( _port );
        env["ZYPP_TEST_DOCROOT"] = _docroot.c_str();
        env["ZYPP_TEST_SOCKPATH"] = socketPath().c_str();
        if ( _ssl )
          env["ZYPP_TEST_USE_SSL"] = "1";
      }

        const char* argv[] =
        {
            "/usr/sbin/lighttpd",
            "-D",
            "-f", confFile.c_str(),
            nullptr
        };

        ExternalTrackedProgram prog( argv, env, ExternalProgram::Stderr_To_Stdout, false, -1, true);
        prog.setBlocking( false );

        //wait some time so we can read config
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        while(1) {
          std::string line = prog.receiveLine();
          if ( line.empty() )
            break;
          std::cerr << line << endl;
        };

        if ( !prog.running() ) {
          _stop = true;
          _stopped = true;
        } else {
          _stopped = false;
        }

        _cv.notify_all();
        if ( _stopped )
          return;

        FCGX_Request request;
        FCGX_InitRequest(&request, sockFD,0);

        struct pollfd fds[] { {
            _wakeupPipe[0],
            POLLIN,
            0
          }, {
            sockFD,
            POLLIN,
            0
          }, {
            fileno( prog.inputFile() ),
            POLLIN | POLLHUP | POLLERR,
            0
          }
        };

        while ( 1 ) {
          //there is no way to give a timeout to FCGX_Accept_r, so we poll the socketfd for new
          //connections and listen on a pipe for a wakeup event to stop the worker if required
          int ret = ::poll( fds, 3, INT_MAX );
          if ( ret == 0 ) {
            //timeout just continue the loop condition will stop if required
            continue;
          }

          if ( ret < 0 ) {
            std::cerr << "Poll error " << endl;
            continue;
          }

          if ( fds[0].revents ) {
            //clear pipe
            char dummy;
            while ( read( _wakeupPipe[0], &dummy, 1 ) > 0 ) { continue; }
          }

          if ( fds[2].revents ) {
            while(1) {
              std::string line = prog.receiveLine();
              if ( line.empty() )
                break;
              std::cerr << line << endl;
            };

            if ( !prog.running() ) {
              std::cerr << "Webserver exited too early" << endl;
              _stop = true;
              _stopped = true;
            }
          }

          if ( _stop )
            break;

          //no events on the listen socket, repoll
          if ( ! ( fds[1].revents & POLLIN ) )
            continue;

          if ( FCGX_Accept_r(&request) == 0 ) {

            fcgi_streambuf cout_fcgi(request.out);
            fcgi_streambuf cin_fcgi(request.in);
            fcgi_streambuf cerr_fcgi(request.err);

            WebServer::Request req( &cin_fcgi, &cout_fcgi, &cerr_fcgi );

            int i = 0;
            for ( char *env = request.envp[i]; env != nullptr; env = request.envp[++i] ) {
              char * eq = strchr( env, '=' );
              if ( !eq ) {
                continue;
              }
              req.params.insert( { std::string( env, eq ), std::string( eq+1 ) } );
            }

            if ( !req.params.count("SCRIPT_NAME") ) {
              req.rerr << "Status: 400 Bad Request\r\n"
                       << "Content-Type: text/html\r\n"
                       << "\r\n"
                       << "Invalid request";
              FCGX_Finish_r(&request);
              continue;
            }

            {
              std::lock_guard<std::mutex> lock( _mut );
              //remove "/handler/" prefix
              std::string key = req.params.at("SCRIPT_NAME").substr( handlerPrefix().length() );
              auto it = _handlers.find( key );
              if ( it == _handlers.end() ) {
                req.rerr << "Status: 404 Not Found\r\n"
                         << "Content-Type: text/html\r\n"
                         << "\r\n"
                         << "Request handler was not found";
                FCGX_Finish_r(&request);
                continue;
              }
              //call the handler
              it->second( req );
              FCGX_Finish_r(&request);
            }
          }
        }
        //prog.close();
        if ( prog.running() )
            prog.kill();
    }

    string log() const
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

      MIL << "Using socket " <<  socketPath() << std::endl;
      _stop = _stopped = false;

      std::unique_lock<std::mutex> lock( _mut );
      _thrd.reset( new std::thread( &Impl::worker_thread, this ) );
      _cv.wait(lock);

      if ( _stopped ) {
        throw zypp::Exception("Failed to start lighttpd");
      }
    }

    std::mutex _mut; //<one mutex to rule em all
    std::condition_variable _cv; //< to sync server startup

    filesystem::TmpDir _workingDir;
    zypp::Pathname _docroot;
    zypp::shared_ptr<std::thread> _thrd;
    std::map<std::string, WebServer::RequestHandler> _handlers;
    unsigned int _port;
    int _wakeupPipe[2];

    std::atomic_bool _stop;
    bool _stopped;
    bool _ssl;
};


WebServer::WebServer(const Pathname &root, unsigned int port, bool useSSL)
    : _pimpl(new Impl(root, port, useSSL))
{
}

void WebServer::start()
{
    _pimpl->start();
}


std::string WebServer::log() const
{
  return _pimpl->log();
}

void WebServer::addRequestHandler( const string &path, RequestHandler &&handler )
{
  std::lock_guard<std::mutex> lock( _pimpl->_mut );
  _pimpl->_handlers[path] = std::move(handler);
}

void WebServer::removeRequestHandler(const string &path)
{
  std::lock_guard<std::mutex> lock( _pimpl->_mut );
  auto it = _pimpl->_handlers.find( path );
  if ( it != _pimpl->_handlers.end() )
    _pimpl->_handlers.erase( it );
}

int WebServer::port() const
{
    return _pimpl->port();
}


Url WebServer::url() const
{
    Url url;
    url.setHost("localhost");
    url.setPort(str::numstring(port()));
    if ( _pimpl->_ssl )
      url.setScheme("https");
    else
      url.setScheme("http");

    return url;
}

media::TransferSettings WebServer::transferSettings() const
{
  media::TransferSettings set;
  set.setCertificateAuthoritiesPath(zypp::Pathname(TESTS_SRC_DIR)/"data/lighttpdconf/ssl/certstore");
  return set;
}

void WebServer::stop()
{
    _pimpl->stop();
}

WebServer::RequestHandler WebServer::makeResponse( std::string resp )  {
  return [ resp ]( WebServer::Request & req ){
    req.rout << resp;
  };
};

WebServer::RequestHandler WebServer::makeResponse( std::string status, std::string content )  {
  return makeResponse( status , std::vector<std::string>(), content );
};

WebServer::RequestHandler WebServer::makeResponse( const std::string &status, const std::vector<std::string> &headers, const std::string &content )  {
  return makeResponse( makeResponseString( status, headers, content ) );
}

string WebServer::makeResponseString(const string &status, const std::vector<string> &headers, const string &content)
{
  static const std::string genericResp = "HTTP/1.1 %1%\r\n"
                                         "%2%"
                                         "\r\n"
                                         "%3%";
  bool hasCType = false;
  for( const std::string &hdr : headers ) {
    if ( zypp::str::startsWith( hdr, "Content-Type:" ) ) {
      hasCType = true;
      break;
    }
  }

  std::string allHeaders;
  if ( !hasCType )
    allHeaders += "Content-Type: text/html; charset=utf-8\r\n";
  allHeaders += zypp::str::join( headers.begin(), headers.end(), "\r\n");

  return ( zypp::str::Format( genericResp ) % status % allHeaders %content );
};

WebServer::~WebServer()
{
}
