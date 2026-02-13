/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "MirroredOrigin.h"

#include <zypp-core/base/LogTools.h>
#include <algorithm>

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::MirroredOrigin"

namespace zypp {

  struct OriginEndpoint::Private {

    Private( Url &&u, OriginEndpoint::SettingsMap &&m )
      : _url(std::move(u))
      , _settings(std::move(m))
    {}
    ~Private() = default;

    Private *clone () const {
      return new Private(*this);
    }

    zypp::Url _url;
    std::unordered_map<std::string, std::any> _settings;
    //OriginEndpoint::SettingsMap _settings;
  };

  OriginEndpoint::OriginEndpoint()
    : _pimpl( new Private(Url(), {} ) )
  {}

  OriginEndpoint::OriginEndpoint( Url url, SettingsMap settings )
    : _pimpl( new Private(std::move(url), std::move(settings) ) )
  {}

  OriginEndpoint::OriginEndpoint( Url url )
    : OriginEndpoint( std::move(url), SettingsMap() )
  { }

  Url &OriginEndpoint::url()
  {
    return _pimpl->_url;
  }

  const Url &OriginEndpoint::url() const
  {
    return _pimpl->_url;
  }

  void OriginEndpoint::setUrl(const Url &newUrl)
  {
    _pimpl->_url = newUrl;
  }

  bool OriginEndpoint::hasConfig(const std::string &key) const
  {
    return (_pimpl->_settings.count (key) > 0);
  }

  std::string OriginEndpoint::scheme() const
  {
    return _pimpl->_url.getScheme();
  }

  bool OriginEndpoint::schemeIsDownloading() const
  {
    return _pimpl->_url.schemeIsDownloading ();
  }

  bool OriginEndpoint::isValid() const
  {
    return _pimpl->_url.isValid();
  }

  void OriginEndpoint::setConfig(const std::string &key, std::any value)
  {
    _pimpl->_settings.insert_or_assign ( key, std::move(value) );
  }

  const std::any &OriginEndpoint::getConfig(const std::string &key) const
  {
    return _pimpl->_settings.at(key);
  }

  std::any &OriginEndpoint::getConfig(const std::string &key)
  {
    return _pimpl->_settings.at(key);
  }

  void OriginEndpoint::eraseConfigValue( const std::string &key )
  {
    auto it = _pimpl->_settings.find (key);
    if ( it == _pimpl->_settings.end() )
      return;
    _pimpl->_settings.erase(it);
  }

  const OriginEndpoint::SettingsMap &OriginEndpoint::config() const
  {
    return _pimpl->_settings;
  }


  OriginEndpoint::SettingsMap &OriginEndpoint::config()
  {
    return _pimpl->_settings;
  }


  std::ostream & operator<<( std::ostream & str, const OriginEndpoint & url )
  {
    return str << url.url().asString();
  }

  bool operator<( const OriginEndpoint &lhs, const OriginEndpoint &rhs )
  {
    return (lhs.url().asCompleteString() < rhs.url().asCompleteString());
  }

  bool operator==( const OriginEndpoint &lhs, const OriginEndpoint &rhs )
  {
    return (lhs.url().asCompleteString() == rhs.url().asCompleteString());
  }

  bool operator!=( const OriginEndpoint &lhs, const OriginEndpoint &rhs )
  {
    return (lhs.url().asCompleteString() != rhs.url().asCompleteString());
  }

  struct MirroredOrigin::Private {
    Private( OriginEndpoint &&authority = {}, std::vector<OriginEndpoint> &&mirrors = {} )
      : _authorities{ std::move(authority) }
      , _mirrors( std::move(mirrors) )
    {}
    ~Private() = default;

    Private *clone () const {
      return new Private(*this);
    }

    bool authoritiesAreValid() const
    {
      if ( _authorities.empty() )
        return false;
      return true;
    }

    std::vector<OriginEndpoint> _authorities;
    std::vector<OriginEndpoint> _mirrors;
  };

  MirroredOrigin::MirroredOrigin()
    : _pimpl( new Private() )
  {}

  MirroredOrigin::MirroredOrigin(OriginEndpoint authority, std::vector<OriginEndpoint> mirrors )
    : _pimpl( new Private( std::move(authority) ) )
  {
    for( auto &m : mirrors ) { addMirror ( std::move(m) ); }
  }

  void MirroredOrigin::setAuthority(OriginEndpoint newAuthority)
  {
    if (!newAuthority.isValid()) {
      MIL << "Skipping authority " << newAuthority << ", is NOT valid" << std::endl;
      return;
    }

    const auto &newScheme = newAuthority.scheme();
    bool newAuthIsDl = newAuthority.url().schemeIsDownloading();

    _pimpl->_authorities.clear();
    _pimpl->_authorities.push_back( std::move(newAuthority) );

    if ( !_pimpl->authoritiesAreValid() || !_pimpl->_mirrors.size () )
      return;

    // house keeeping, we want only compatible mirrors
    for ( auto i = _pimpl->_mirrors.begin (); i != _pimpl->_mirrors.end(); ) {
      if (    ( newAuthIsDl && !i->schemeIsDownloading() ) // drop mirror if its not downloading but authority is
           && ( i->scheme () != newScheme )                // otherwise drop if scheme is not identical
      ) {
        MIL << "Dropping mirror " << *i << " scheme is not compatible to new authority URL ( " << i->scheme() << " vs " << newScheme << ")" << std::endl;
        i = _pimpl->_mirrors.erase(i);
      } else {
        i++;
      }
    }
  }

  const std::vector<OriginEndpoint> &MirroredOrigin::authorities() const
  {
    return _pimpl->_authorities;
  }

  OriginEndpoint MirroredOrigin::authority() const
  {
    if (_pimpl->_authorities.empty()) {
      return OriginEndpoint();
    }
    return _pimpl->_authorities[0];
  }

  const std::vector<OriginEndpoint> &MirroredOrigin::mirrors() const
  {
    return _pimpl->_mirrors;
  }

  bool MirroredOrigin::isValid() const
  {
    return _pimpl->authoritiesAreValid();
  }

  bool MirroredOrigin::addAuthority(OriginEndpoint newAuthority)
  {
    if (!newAuthority.isValid()) {
      MIL << "Skipping authority " << newAuthority << ", is NOT valid" << std::endl;
      return false;
    }

    if ( _pimpl->authoritiesAreValid() ) {
      const auto &authScheme = _pimpl->_authorities[0].scheme();
      bool authIsDl = _pimpl->_authorities[0].schemeIsDownloading();

      if ( ( authIsDl && !newAuthority.schemeIsDownloading () )
           && ( authScheme != newAuthority.scheme () )
      ) {
        MIL << "Ignoring authority " << newAuthority << " scheme is not compatible to current authority URL ( " << newAuthority.scheme() << " vs " << authScheme << ")" << std::endl;
        return false;
      }
    }

    if (std::find(_pimpl->_authorities.begin(), _pimpl->_authorities.end(), newAuthority) != _pimpl->_authorities.end()) {
      MIL << "Ignoring authority " << newAuthority << " already present in authorities" << std::endl;
      return true;
    }

    _pimpl->_authorities.push_back( std::move(newAuthority) );
    return true;
  }

  bool MirroredOrigin::addMirror(OriginEndpoint newMirror)
  {
    if ( _pimpl->authoritiesAreValid() ) {
      const auto &authScheme = _pimpl->_authorities[0].scheme();
      bool authIsDl = _pimpl->_authorities[0].schemeIsDownloading();

      if ( ( authIsDl && !newMirror.schemeIsDownloading () )
           && ( authScheme != newMirror.scheme () )
      ) {
        MIL << "Ignoring mirror " << newMirror << " scheme is not compatible to authority URL ( " << newMirror.scheme() << " vs " << authScheme << ")" << std::endl;
        return false;
      }
    }

    if (std::find(_pimpl->_authorities.begin(), _pimpl->_authorities.end(), newMirror) != _pimpl->_authorities.end()) {
      MIL << "Ignoring mirror " << newMirror << " already present in authorities" << std::endl;
      return true;
    }

    if (std::find(_pimpl->_mirrors.begin(), _pimpl->_mirrors.end(), newMirror) != _pimpl->_mirrors.end()) {
      MIL << "Ignoring mirror " << newMirror << " already present in mirrors" << std::endl;
      return true;
    }

    _pimpl->_mirrors.push_back( std::move(newMirror) );
    return true;
  }

  void MirroredOrigin::setMirrors(std::vector<OriginEndpoint> mirrors)
  {
    clearMirrors();
    for ( auto &m : mirrors )
      addMirror( std::move(m) );
  }

  void MirroredOrigin::clearMirrors()
  {
    _pimpl->_mirrors.clear();
  }

  std::string MirroredOrigin::scheme() const
  {
    if ( _pimpl->_authorities.empty() )
      return std::string();
    return _pimpl->_authorities[0].url().getScheme();
  }

  bool MirroredOrigin::schemeIsDownloading() const
  {
    if ( _pimpl->_authorities.empty() )
      return false;
    return _pimpl->_authorities[0].schemeIsDownloading();
  }

  uint MirroredOrigin::endpointCount() const
  {
    return _pimpl->_authorities.size() + _pimpl->_mirrors.size();
  }

  uint MirroredOrigin::authorityCount() const
  {
    return _pimpl->_authorities.size();
  }

  const OriginEndpoint &MirroredOrigin::at(uint index) const
  {
    if ( index >= endpointCount() ) {
      throw std::out_of_range( "OriginEndpoint index out of range." );
    }
    if ( index < _pimpl->_authorities.size() ) {
      return _pimpl->_authorities[index];
    }

    return _pimpl->_mirrors.at( index - _pimpl->_authorities.size() );
  }

  OriginEndpoint &MirroredOrigin::at(uint index)
  {
    if ( index >= endpointCount() ) {
      throw std::out_of_range( "OriginEndpoint index out of range." );
    }
    if ( index < _pimpl->_authorities.size() ) {
      return _pimpl->_authorities[index];
    }

    return _pimpl->_mirrors.at( index - _pimpl->_authorities.size() );
  }

  struct MirroredOriginSet::Private
  {
    Private() {}
    ~Private() = default;

    Private *clone () const {
      return new Private(*this);
    }

    std::optional<std::size_t>  _dlIndex; //< set if there is a downloading MirroredOrigin
    std::vector<MirroredOrigin> _mirrors;
  };


  MirroredOriginSet::MirroredOriginSet()
      : _pimpl( new Private() )
  {}

  MirroredOriginSet::MirroredOriginSet( std::vector<OriginEndpoint> eps )
    : MirroredOriginSet()
  {
    if ( eps.size() )
      addEndpoints( std::move(eps) );
  }

  MirroredOriginSet::MirroredOriginSet(std::list<Url> urls)
    : MirroredOriginSet()
  {
    for( auto &url: urls )
      addEndpoint( std::move(url) );
  }

  MirroredOriginSet::MirroredOriginSet(std::vector<Url> urls)
    : MirroredOriginSet()
  {
    for( auto &url: urls )
      addEndpoint( std::move(url) );
  }

  const MirroredOrigin &MirroredOriginSet::at(size_type idx) const
  {
    return _pimpl->_mirrors.at(idx);
  }

  MirroredOrigin &MirroredOriginSet::at(size_type idx)
  {
    return _pimpl->_mirrors.at(idx);
  }

  std::ostream & operator<<( std::ostream & str, const MirroredOrigin & origin )
  {
    str << "MirroredOrigin { ";
    dumpRange( str, origin.authorities().begin(), origin.authorities().end(), "authorities: [", "\"", "\",\"", "\"", "]" );
    str << ", ";
    dumpRange( str, origin.mirrors().begin(), origin.mirrors().end(), "mirrors: [", "\"", "\",\"", "\"", "]" );
    return str << " }";
  }

  MirroredOriginSet::iterator MirroredOriginSet::findByUrl( const Url &url )
  {
    for ( auto i = begin(); i!=end(); i++ ) {
      auto epI = std::find_if( i->begin (), i->end(), [&](const OriginEndpoint &ep){ return ep.url () == url; } );
      if ( epI != i->end() )
        return i;
    }
    return end();
  }

  MirroredOriginSet::const_iterator MirroredOriginSet::findByUrl( const Url &url ) const
  {
    for ( auto i = begin(); i!=end(); i++ ) {
      auto epI = std::find_if( i->begin (), i->end(), [&](const OriginEndpoint &ep){ return ep.url () == url; } );
      if ( epI != i->end() )
        return i;
    }
    return end();
  }

  void MirroredOriginSet::addAuthorityEndpoint( OriginEndpoint endpoint )
  {
    if ( !endpoint.url().schemeIsDownloading () ) {
      _pimpl->_mirrors.push_back ( MirroredOrigin(std::move(endpoint), {} ) );
      return;
    }

    if ( _pimpl->_dlIndex ) {
      _pimpl->_mirrors.at(*_pimpl->_dlIndex).addAuthority( std::move(endpoint) );
      return;
    }

    // start a new origin
    _pimpl->_mirrors.push_back ( MirroredOrigin(std::move(endpoint), {} ) );
    _pimpl->_dlIndex = _pimpl->_mirrors.size() - 1;
  }

  void MirroredOriginSet::addEndpoint( OriginEndpoint endpoint )
  {
    if ( !endpoint.url().schemeIsDownloading () ) {
      _pimpl->_mirrors.push_back ( MirroredOrigin(std::move(endpoint), {} ) );
      return;
    }

    if ( _pimpl->_dlIndex ) {
      _pimpl->_mirrors.at(*_pimpl->_dlIndex).addMirror( std::move(endpoint) );
      return;
    }

    // start a new origin
    _pimpl->_mirrors.push_back ( MirroredOrigin(std::move(endpoint), {} ) );
    _pimpl->_dlIndex = _pimpl->_mirrors.size() - 1;
  }


  void MirroredOriginSet::addEndpoints( std::vector<OriginEndpoint> endpoints )
  {
    for ( auto &ep : endpoints )
      addEndpoint ( std::move(ep) );
  }

  bool MirroredOriginSet::empty() const
  {
    return _pimpl->_mirrors.empty ();
  }

  void MirroredOriginSet::clear()
  {
    _pimpl->_mirrors.clear();
    _pimpl->_dlIndex.reset();
  }

  MirroredOriginSet::iterator MirroredOriginSet::begin()
  {
    return _pimpl->_mirrors.begin ();
  }


  MirroredOriginSet::iterator MirroredOriginSet::end()
  {
    return _pimpl->_mirrors.end ();
  }


  MirroredOriginSet::const_iterator MirroredOriginSet::begin() const
  {
    return _pimpl->_mirrors.begin ();
  }


  MirroredOriginSet::const_iterator MirroredOriginSet::end() const
  {
    return _pimpl->_mirrors.end ();
  }

  MirroredOriginSet::size_type MirroredOriginSet::size() const
  {
    return _pimpl->_mirrors.size ();
  }

  bool MirroredOriginSet::hasFallbackUrls() const
  {
    return ( size() == 1 && at( 0 ).endpointCount() > 1 ) || size() > 1;
  }

  std::ostream & operator<<( std::ostream & str, const MirroredOriginSet & origin )
  {
    return dumpRange( str, origin.begin(), origin.end(), "MirroredOriginSet {", " ", ", ", " ", "}" );
  }

}
