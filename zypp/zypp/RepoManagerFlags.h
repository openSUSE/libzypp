/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/RepoManager.h
 *
*/
#ifndef ZYPP_REPOMANAGER_FLAGS_INCLUDED_H
#define ZYPP_REPOMANAGER_FLAGS_INCLUDED_H

#include <zypp-core/base/Flags.h>

namespace zypp {
  namespace RepoManagerFlags {

    enum RawMetadataRefreshPolicy
    {
      RefreshIfNeeded,
      RefreshForced,
      RefreshIfNeededIgnoreDelay
    };

    enum CacheBuildPolicy
    {
      BuildIfNeeded,
      BuildForced
    };

    /** Flags for tuning RefreshService */
    enum RefreshServiceBit
    {
      RefreshService_restoreStatus	= (1<<0),	///< Force restoring repo enabled/disabled status
      RefreshService_forceRefresh	= (1<<1),	///< Force refresh even if TTL is not reached
    };
    ZYPP_DECLARE_FLAGS(RefreshServiceFlags,RefreshServiceBit);

    /** Options tuning RefreshService */
    using RefreshServiceOptions = RefreshServiceFlags;

    /**
     * Possibly return state of RepoManager::checkIfToRefreshMetadata function
     */
    enum RefreshCheckStatus {
      REFRESH_NEEDED,     /**< refresh is needed */
      REPO_UP_TO_DATE,    /**< repository not changed */
      REPO_CHECK_DELAYED  /**< refresh is delayed due to settings */
    };
  }

  ZYPP_DECLARE_OPERATORS_FOR_FLAGS( RepoManagerFlags::RefreshServiceFlags );
}
#endif  //ZYPP_REPOMANAGER_FLAGS_INCLUDED_H
