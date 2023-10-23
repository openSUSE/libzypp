/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_PRIVATE_REPOMANAGER_P_H
#define ZYPP_NG_PRIVATE_REPOMANAGER_P_H

#include <zypp/ng/repomanager.h>
#include <zypp/zypp_detail/repomanagerbase_p.h>
#include <zypp-core/zyppng/base/private/base_p.h>

namespace zyppng {

  class ZYPP_LOCAL RepoManagerPrivate : public BasePrivate, public zypp::RepoManagerBaseImpl
  {
    ZYPP_DECLARE_PUBLIC(RepoManager)
  public:

    RepoManagerPrivate( ContextRef ctx, RepoManagerOptions repoOpts, RepoManager &p );

    AsyncOpRef<expected<zypp::repo::RepoType>> probe( const zypp::Url & url, const zypp::Pathname & path = zypp::Pathname() ) const;

    ContextWeakRef _context; //< weak ref to our context to prevent a ref loop, this should always be valid

    // RepoManagerBaseImpl interface
  public:
    void removeRepository(const zypp::RepoInfo &info, const zypp::ProgressData::ReceiverFnc &) override;
  };

}


#endif
