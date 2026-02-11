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
      : _authority( std::move(authority) )
      , _origins( std::move(mirrors) )
    {}
    ~Private() = default;

    Private *clone () const {
      return new Private(*this);
    }

    OriginEndpoint _authority;
    std::vector<OriginEndpoint> _origins;
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
    const auto &newScheme = newAuthority.scheme();
    bool newAuthIsDl = newAuthority.url().schemeIsDownloading();

    _pimpl->_authority = std::move(newAuthority);

    if ( !_pimpl->_authority.isValid() || !_pimpl->_origins.size () )
      return;

    // house keeeping, we want only compatible mirrors
    for ( auto i = _pimpl->_origins.begin (); i != _pimpl->_origins.end(); ) {
      if (    ( newAuthIsDl && !i->schemeIsDownloading() ) // drop mirror if its not downloading but authority is
           && ( i->scheme () != newScheme )                // otherwise drop if scheme is not identical
      ) {
        MIL << "Dropping mirror " << *i << " scheme is not compatible to new authority URL ( " << i->scheme() << " vs " << newScheme << ")" << std::endl;
        i = _pimpl->_origins.erase(i);
      } else {
        i++;
      }
    }
  }

  const OriginEndpoint &MirroredOrigin::authority() const
  {
    return _pimpl->_authority;
  }

  const std::vector<OriginEndpoint> &MirroredOrigin::mirrors() const
  {
    return _pimpl->_origins;
  }

  bool MirroredOrigin::isValid() const
  {
    return _pimpl->_authority.isValid();
  }

  bool MirroredOrigin::addMirror(OriginEndpoint newMirror)
  {
    if ( _pimpl->_authority.isValid()
         && ( _pimpl->_authority.schemeIsDownloading() && !newMirror.schemeIsDownloading () )
         && ( _pimpl->_authority.scheme () != newMirror.scheme () )

    ) {
      MIL << "Ignoring mirror " << newMirror << " scheme is not compatible to new authority URL ( " << newMirror.scheme() << " vs " << _pimpl->_authority.scheme() << ")" << std::endl;
      return false;
    }
    _pimpl->_origins.push_back( std::move(newMirror) );
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
    _pimpl->_origins.clear();
  }

  std::string MirroredOrigin::scheme() const
  {
    return _pimpl->_authority.url().getScheme();
  }

  bool MirroredOrigin::schemeIsDownloading() const
  {
    return _pimpl->_authority.schemeIsDownloading();
  }

  uint MirroredOrigin::endpointCount() const
  {
    // authority is always accessible, even if its a invalid URL
    return _pimpl->_origins.size() + 1;
  }

  const OriginEndpoint &MirroredOrigin::at(uint index) const
  {
    if ( index >= endpointCount() ) {
      throw std::out_of_range( "OriginEndpoint index out of range." );
    }
    if ( index == 0 ) {
      return _pimpl->_authority;
    }

    return _pimpl->_origins.at( index - 1 );
  }

  OriginEndpoint &MirroredOrigin::at(uint index)
  {
    if ( index >= endpointCount() ) {
      throw std::out_of_range( "OriginEndpoint index out of range." );
    }
    if ( index == 0 ) {
      return _pimpl->_authority;
    }

    return _pimpl->_origins.at( index - 1 );
  }

  struct MirroredOriginSet::Private
  {
    Private() {}
    ~Private() = default;

    Private *clone () const {
      return new Private(*this);
    }

    std::optional<std::size_t>  _dlIndex; //< set if there is a downloading MirroredOrigin
    std::vector<MirroredOrigin> _origins;
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
    return _pimpl->_origins.at(idx);
  }

  MirroredOrigin &MirroredOriginSet::at(size_type idx)
  {
    return _pimpl->_origins.at(idx);
  }

  std::ostream & operator<<( std::ostream & str, const MirroredOrigin & origin )
  {
    return dumpRange( str << "MirroredOrigin { authority: \"" << origin.authority() << "\", ",
                      origin.mirrors().begin(), origin.mirrors().end(), "mirrors: [", "\"", "\",\"", "\"", "]" )
    << " }";
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

  void MirroredOriginSet::addEndpoint( OriginEndpoint endpoint )
  {
    if ( !endpoint.url().schemeIsDownloading () ) {
      _pimpl->_origins.push_back ( MirroredOrigin(std::move(endpoint), {} ) );
      return;
    }

    if ( _pimpl->_dlIndex ) {
      _pimpl->_origins.at(*_pimpl->_dlIndex).addMirror( std::move(endpoint) );
      return;
    }

    // start a new origin
    _pimpl->_origins.push_back ( MirroredOrigin(std::move(endpoint), {} ) );
    _pimpl->_dlIndex = _pimpl->_origins.size() - 1;
  }


  void MirroredOriginSet::addEndpoints( std::vector<OriginEndpoint> endpoints )
  {
    for ( auto &ep : endpoints )
      addEndpoint ( std::move(ep) );
  }

  bool MirroredOriginSet::empty() const
  {
    return _pimpl->_origins.empty ();
  }

  void MirroredOriginSet::clear()
  {
    _pimpl->_origins.clear();
    _pimpl->_dlIndex.reset();
  }

  MirroredOriginSet::iterator MirroredOriginSet::begin()
  {
    return _pimpl->_origins.begin ();
  }


  MirroredOriginSet::iterator MirroredOriginSet::end()
  {
    return _pimpl->_origins.end ();
  }


  MirroredOriginSet::const_iterator MirroredOriginSet::begin() const
  {
    return _pimpl->_origins.begin ();
  }


  MirroredOriginSet::const_iterator MirroredOriginSet::end() const
  {
    return _pimpl->_origins.end ();
  }

  MirroredOriginSet::size_type MirroredOriginSet::size() const
  {
    return _pimpl->_origins.size ();
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
