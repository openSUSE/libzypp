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


  AttachedMediaInfo::AttachedMediaInfo(const std::string &id, ProvideQueue::Config::WorkerType workerType, const zypp::MirroredOrigin &originConfig, ProvideMediaSpec &spec )
    : AttachedMediaInfo( id, {}, workerType, originConfig, spec )
  { }

  AttachedMediaInfo::AttachedMediaInfo(const std::string &id, ProvideQueueWeakRef backingQueue, ProvideQueue::Config::WorkerType workerType, const zypp::MirroredOrigin &originConfig, const ProvideMediaSpec &mediaSpec , const std::optional<zypp::Pathname> &mnt )
    : _name(id)
    , _backingQueue( std::move(backingQueue) )
    , _workerType( workerType )
    , _originConfig( originConfig )
    , _spec( mediaSpec )
    , _localMountPoint( mnt )
  {
    // idle on construction, since only the Provide has a reference atm
    _idleSince = std::chrono::steady_clock::now();
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
    if ( !_originConfig.isValid() )
      return {};

    return _originConfig.authority().url();
  }

  void AttachedMediaInfo::unref_to(unsigned int refCnt) const {
    // last reference is always owned by the Provide instance
    if ( refCnt == 1 )
      _idleSince = std::chrono::steady_clock::now();
  }

  void AttachedMediaInfo::ref_to(unsigned int refCnt) const {
    if ( _idleSince && refCnt > 1 ) _idleSince.reset();
  }

  bool AttachedMediaInfo::isSameMedium(const zypp::MirroredOrigin &origin, const ProvideMediaSpec &spec) {
    return isSameMedium( _originConfig, _spec, origin, spec );
  }

  bool AttachedMediaInfo::isSameMedium( const zypp::MirroredOrigin &originA, const ProvideMediaSpec &specA, const zypp::MirroredOrigin &originB, const ProvideMediaSpec &specB )
  {
    const auto check = specA.isSameMedium(specB);
    if ( !zypp::indeterminate (check) )
      return (bool)check;

    // if the endpoints intersect we assume same medium
    const auto &intersects = [](  const zypp::MirroredOrigin &l1, const zypp::MirroredOrigin &l2 ){
      bool intersect = false;
      for ( const auto &u: l1 ) {
        intersect = ( std::find( l2.begin (), l2.end(), u ) != l2.end() );
        if ( intersect )
          break;
      }
      return intersect;
    };

    return intersects( originA, originB );
  }

}
