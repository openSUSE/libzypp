/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\----------------------------------------------------------------------/
*/
#include "private/attachedmediainfo_p.h"

namespace zyppng {

  IMPL_PTR_TYPE( AttachedMediaInfo )


  AttachedMediaInfo::AttachedMediaInfo(const std::string &id, ProvideQueue::Config::WorkerType workerType, const zypp::Url &baseUrl, const std::vector<zypp::Url> &mirrors, ProvideMediaSpec &spec )
    : AttachedMediaInfo( id, {}, workerType, baseUrl, mirrors, spec )
  { }

  AttachedMediaInfo::AttachedMediaInfo(const std::string &id, ProvideQueueWeakRef backingQueue, ProvideQueue::Config::WorkerType workerType, const zypp::Url &baseUrl, const std::vector<zypp::Url> &mirrors, const ProvideMediaSpec &mediaSpec , const std::optional<zypp::Pathname> &mnt )
    : _name(id)
    , _backingQueue( std::move(backingQueue) )
    , _workerType( workerType )
    , _mirrors( mirrors )
    , _spec( mediaSpec )
    , _localMountPoint( mnt )
  {
    // idle on construction, since only the Provide has a reference atm
    _idleSince = std::chrono::steady_clock::now();

    // make sure attach URL is in front
    if ( _mirrors.empty () ) {
      _mirrors.push_back ( baseUrl );
    } else {
      auto i = std::find( _mirrors.begin(), _mirrors.end(), baseUrl );
      if ( i != _mirrors.begin() ) {
        if ( i != _mirrors.end() )
          _mirrors.erase(i);
        _mirrors.insert( _mirrors.begin(), baseUrl );
      }
    }
  }

  void AttachedMediaInfo::setName(std::string &&name)
  {
    _name = std::move(name);
  }

  const std::string &AttachedMediaInfo::name() const
  {
    return _name;
  }

  zypp::Url AttachedMediaInfo::attachedUrl() const
  {
    if ( !_mirrors.size() )
      return {};

    return _mirrors.at(0);
  }

  void AttachedMediaInfo::unref_to(unsigned int refCnt) const {
    // last reference is always owned by the Provide instance
    if ( refCnt == 1 )
      _idleSince = std::chrono::steady_clock::now();
  }

  void AttachedMediaInfo::ref_to(unsigned int refCnt) const {
    if ( _idleSince && refCnt > 1 ) _idleSince.reset();
  }

  bool AttachedMediaInfo::isSameMedium(const std::vector<zypp::Url> &urls, const ProvideMediaSpec &spec) {
    return isSameMedium( _mirrors, _spec, urls, spec );
  }

  bool AttachedMediaInfo::isSameMedium(const std::vector<zypp::Url> &mirrorsA, const ProvideMediaSpec &specA, const std::vector<zypp::Url> &mirrorsB, const ProvideMediaSpec &specB)
  {
    const auto check = specA.isSameMedium(specB);
    if ( !zypp::indeterminate (check) )
      return (bool)check;

    // if the mirrors intersect we assume same medium
    const auto &intersects = []( const std::vector<zypp::Url> &l1, const std::vector<zypp::Url> &l2 ){
      bool intersect = false;
      for ( const auto &u: l1 ) {
        intersect = ( std::find( l2.begin (), l2.end(), u ) != l2.end() );
        if ( intersect )
          break;
      }
      return intersect;
    };

    return intersects( mirrorsA, mirrorsB );
  }

}
