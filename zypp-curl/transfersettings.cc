/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-curl/transfersettings.cc
 *
*/

#include "transfersettings.h"
#include <iostream>
#include <sstream>

#include <zypp-core/base/String.h>
#include <zypp-core/base/Logger.h>
#include <zypp-core/fs/WatchFile>
#include <zypp-core/base/ReferenceCounted.h>
#include <zypp-core/base/NonCopyable.h>
#include <zypp-core/ExternalProgram.h>
#include <zypp-media/MediaConfig>

#include <zypp-core/Globals.h>

using std::endl;

#define CURL_BINARY "/usr/bin/curl"

namespace zypp
{
  namespace media
  {
    class TransferSettings::Impl
    {
    public:
      Impl() : _useproxy( false ),
        _timeout( MediaConfig::instance().download_transfer_timeout() ),
        _connect_timeout( MediaConfig::instance().download_connect_timeout() ),
        _maxConcurrentConnections( MediaConfig::instance().download_max_concurrent_connections() ),
        _minDownloadSpeed(MediaConfig::instance().download_min_download_speed()),
        _maxDownloadSpeed(MediaConfig::instance().download_max_download_speed()),
        _maxSilentTries(MediaConfig::instance().download_max_silent_tries() ),
        _verify_host(false),
        _verify_peer(false),
        _ca_path("/etc/ssl/certs"),
        _enableCookieFile(false),
        _head_requests_allowed(true)
      {}

      Impl(const Impl &) = default;
      Impl(Impl &&) = default;
      Impl &operator=(const Impl &) = delete;
      Impl &operator=(Impl &&) = delete;
      virtual ~Impl() {}

      /** Offer default Impl. */
      static shared_ptr<Impl> nullimpl()
      {
        static shared_ptr<Impl> _nullimpl( new Impl );
        return _nullimpl;
      }

    private:
      friend Impl * rwcowClone<Impl>( const Impl * rhs );
      /** clone for RWCOW_pointer */
      Impl * clone() const
      { return new Impl( *this ); }

    public:
      void safeAddHeader( std::string val_r ) {
        // bsc#1212187: HTTP/2 RFC 9113 forbids fields ending with a space.
        // Trim and discard empty header.
        val_r = str::trim( std::move(val_r) );
        if ( not val_r.empty() )
          _headers.push_back( std::move(val_r) );
        else
          WAR << "Discard empty header" << endl;
      }

    public:
      std::vector<std::string> _headers;
      std::string _useragent;
      std::string _username;
      std::string _password;
      bool _useproxy;
      std::string _proxy;
      std::string _proxy_username;
      std::string _proxy_password;
      std::string _authtype;
      long _timeout;
      long _connect_timeout;
      Url _url;
      Pathname _targetdir;

      long _maxConcurrentConnections;
      long _minDownloadSpeed;
      long _maxDownloadSpeed;
      long _maxSilentTries;

      bool _verify_host;
      bool _verify_peer;
      Pathname _ca_path;
      Pathname _client_cert_path;
      Pathname _client_key_path;

      bool _enableCookieFile;

      // workarounds
      bool _head_requests_allowed;
    };

    TransferSettings::TransferSettings()
    : _impl(new TransferSettings::Impl())
    {}

    void TransferSettings::reset()
    { _impl.reset(new TransferSettings::Impl()); }


    void TransferSettings::addHeader( const std::string & val_r )
    { _impl->safeAddHeader( val_r ); }
    void TransferSettings::addHeader( std::string && val_r )
    { _impl->safeAddHeader( std::move(val_r) ); }

    const TransferSettings::Headers &TransferSettings::headers() const
    {
      //@TODO check if we could use a vector of std::string_view here
      return _impl->_headers;
    }

    void TransferSettings::setUserAgentString( const std::string &val_r )
    { _impl->_useragent = str::trim( val_r ); }  // bsc#1212187: HTTP/2 RFC 9113 forbids fields ending with a space

    void TransferSettings::setUserAgentString( std::string && val_r )
    { _impl->_useragent = str::trim( std::move(val_r) ); }  // bsc#1212187: HTTP/2 RFC 9113 forbids fields ending with a space

    const std::string &TransferSettings::userAgentString() const
    { return _impl->_useragent; }


    void TransferSettings::setUsername( const std::string &val_r )
    { _impl->_username = val_r; }

    void TransferSettings::setUsername( std::string && val_r )
    { _impl->_username = std::move(val_r); }

    const std::string &TransferSettings::username() const
    { return _impl->_username; }

    void TransferSettings::setPassword( const std::string & val_r )
    { _impl->_password = val_r; }

    void TransferSettings::setPassword( std::string && val_r )
    { _impl->_password = std::move(val_r); }

    const std::string &TransferSettings::password() const
    { return _impl->_password; }

    std::string TransferSettings::userPassword() const
    {
      std::string userpwd = username();
      if ( password().size() ) {
        userpwd += ":" + password();
      }
      return userpwd;
    }

    void TransferSettings::setAnonymousAuth()
    {
      setUsername("anonymous");
      setPassword("yast@" LIBZYPP_VERSION_STRING);
    }


    void TransferSettings::setProxyEnabled( bool enabled )
    { _impl->_useproxy = enabled; }

    bool TransferSettings::proxyEnabled() const
    { return _impl->_useproxy; }


    void TransferSettings::setProxy( const std::string &val_r )
    { _impl->_proxy = val_r; }

    void TransferSettings::setProxy( std::string && val_r )
    { _impl->_proxy = std::move(val_r); }

    const std::string &TransferSettings::proxy() const
    { return _impl->_proxy; }


    void TransferSettings::setProxyUsername( const std::string &val_r )
    { _impl->_proxy_username = val_r; }

    void TransferSettings::setProxyUsername( std::string && val_r )
    { _impl->_proxy_username = std::move(val_r); }

    const std::string &TransferSettings::proxyUsername() const
    { return _impl->_proxy_username; }

    void TransferSettings::setProxyPassword( const std::string &val_r )
    { _impl->_proxy_password = val_r; }

    void TransferSettings::setProxyPassword( std::string && val_r )
    { _impl->_proxy_password = std::move(val_r); }

    const std::string &TransferSettings::proxyPassword() const
    { return _impl->_proxy_password; }

    std::string TransferSettings::proxyUserPassword() const
    {
      std::string userpwd = proxyUsername();
      if ( proxyPassword().size() ) {
        userpwd += ":" + proxyPassword();
      }
      return userpwd;
    }


    void TransferSettings::setTimeout( long t )
    { _impl->_timeout = (t); }

    long TransferSettings::timeout() const
    { return _impl->_timeout; }


    void TransferSettings::setConnectTimeout( long t )
    { _impl->_connect_timeout = (t); }

    long TransferSettings::connectTimeout() const
    { return _impl->_connect_timeout; }


    void TransferSettings::setMaxConcurrentConnections( long v )
    { _impl->_maxConcurrentConnections = (v); }

    long TransferSettings::maxConcurrentConnections() const
    { return _impl->_maxConcurrentConnections; }


    void TransferSettings::setMinDownloadSpeed( long v )
    { _impl->_minDownloadSpeed = (v); }

    long TransferSettings::minDownloadSpeed() const
    { return _impl->_minDownloadSpeed; }


    void TransferSettings::setMaxDownloadSpeed( long v )
    { _impl->_maxDownloadSpeed = (v); }

    long TransferSettings::maxDownloadSpeed() const
    { return _impl->_maxDownloadSpeed; }


    void TransferSettings::setMaxSilentTries( long v )
    { _impl->_maxSilentTries = (v); }

    long TransferSettings::maxSilentTries() const
    { return _impl->_maxSilentTries; }


    void TransferSettings::setVerifyHostEnabled( bool enabled )
    { _impl->_verify_host = (enabled); }

    bool TransferSettings::verifyHostEnabled() const
    { return _impl->_verify_host; }


    void TransferSettings::setVerifyPeerEnabled( bool enabled )
    { _impl->_verify_peer = enabled; }

    bool TransferSettings::verifyPeerEnabled() const
    { return _impl->_verify_peer; }

    void TransferSettings::setClientCertificatePath( const Pathname &val_r )
    { _impl->_client_cert_path = val_r; }

    void TransferSettings::setClientCertificatePath( Pathname && val_r )
    { _impl->_client_cert_path = std::move( val_r ); }

    const Pathname &TransferSettings::clientCertificatePath() const
    { return _impl->_client_cert_path; }


    void TransferSettings::setClientKeyPath( const Pathname &val_r )
    { _impl->_client_key_path = val_r; }

    void TransferSettings::setClientKeyPath( Pathname && val_r )
    { _impl->_client_key_path = std::move( val_r ); }

    const Pathname &TransferSettings::clientKeyPath() const
    { return _impl->_client_key_path; }

    void TransferSettings::setEnableCookieFile(bool enable)
    { _impl->_enableCookieFile = enable; }

    bool TransferSettings::cookieFileEnabled() const
    { return _impl->_enableCookieFile; }

    void TransferSettings::setCertificateAuthoritiesPath( const Pathname &val_r )
    { _impl->_ca_path = val_r; }

    void TransferSettings::setCertificateAuthoritiesPath( Pathname && val_r )
    { _impl->_ca_path = std::move(val_r); }

    const Pathname &TransferSettings::certificateAuthoritiesPath() const
    { return _impl->_ca_path; }


    void TransferSettings::setAuthType( const std::string &val_r )
    { _impl->_authtype = val_r; }

    void TransferSettings::setAuthType( std::string && val_r )
    { _impl->_authtype = std::move(val_r); }

    const std::string &TransferSettings::authType() const
    { return _impl->_authtype; }


    void TransferSettings::setHeadRequestsAllowed( bool allowed )
    { _impl->_head_requests_allowed = allowed; }

    bool TransferSettings::headRequestsAllowed() const
    { return _impl->_head_requests_allowed; }

  } // namespace media
} // namespace zypp
