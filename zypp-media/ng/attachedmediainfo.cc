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


  AttachedMediaInfo::AttachedMediaInfo( const std::string &id, zypp::proto::Capabilities::WorkerType workerType, const zypp::Url &baseUrl, ProvideMediaSpec &spec )
    : AttachedMediaInfo( id, {}, workerType, baseUrl, spec )
  { }

  AttachedMediaInfo::AttachedMediaInfo( const std::string &id, ProvideQueueWeakRef backingQueue, zypp::proto::Capabilities::WorkerType workerType, const zypp::Url &baseUrl, const ProvideMediaSpec &mediaSpec , const std::optional<zypp::Pathname> &mnt )
    : _name(id)
    , _backingQueue( std::move(backingQueue) )
    , _workerType( workerType )
    , _attachedUrl( baseUrl )
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

  void AttachedMediaInfo::unref_to(unsigned int refCnt) const {
    // last reference is always owned by the Provide instance
    if ( refCnt == 1 )
      _idleSince = std::chrono::steady_clock::now();
  }

  void AttachedMediaInfo::ref_to(unsigned int refCnt) const {
    if ( _idleSince && refCnt > 1 ) _idleSince.reset();
  }
  bool AttachedMediaInfo::isSameMedium(const std::vector<zypp::Url> &urls, const ProvideMediaSpec &spec) {

    const auto check = _spec.isSameMedium(spec);
    if ( !zypp::indeterminate (check) )
      return (bool)check;

    // let the URL rule
    return ( std::find( urls.begin(), urls.end(), _attachedUrl ) != urls.end() );
  }

}
