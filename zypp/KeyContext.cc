/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "KeyContext.h"

#include <zypp/ng/repoinfo.h>
#include <zypp/RepoInfo.h>

namespace zypp {

  class KeyContext::Impl {

  public:
    Impl(){}
    Impl( zyppng::RepoInfo ri ) : _repoInfo( std::move(ri) ) {}

    std::optional<zyppng::RepoInfo> _repoInfo;

  private:
    friend KeyContext::Impl * zypp::rwcowClone<KeyContext::Impl>( const KeyContext::Impl * rhs );
    Impl * clone() const { return new Impl( *this ); }
  };

  ZYPP_BEGIN_LEGACY_API
  KeyContext::KeyContext(const RepoInfo &repoinfo) : KeyContext( repoinfo.ngRepoInfo() )
  { }

  const RepoInfo KeyContext::repoInfo() const
  {
    if ( _pimpl->_repoInfo ) {
      return RepoInfo( *_pimpl->_repoInfo );
    }
    return RepoInfo();
  }
  ZYPP_END_LEGACY_API

  void KeyContext::setRepoInfo(const RepoInfo &repoinfo)
  {
    _pimpl->_repoInfo = repoinfo.ngRepoInfo();
  }

  KeyContext::KeyContext() : _pimpl(new Impl()) {}
  KeyContext::KeyContext(const zyppng::RepoInfo &repoinfo)
    : _pimpl( new Impl(repoinfo) )
  { }

  bool KeyContext::empty() const {
    return (!_pimpl->_repoInfo.has_value()) || _pimpl->_repoInfo->alias().empty();
  }
  void KeyContext::setRepoInfo( const zyppng::RepoInfo &repoinfo )
  {
    _pimpl->_repoInfo = repoinfo;
  }
  const std::optional<zyppng::RepoInfo> &KeyContext::ngRepoInfo() const {
    return _pimpl->_repoInfo;
  }
} // namespace zypp
