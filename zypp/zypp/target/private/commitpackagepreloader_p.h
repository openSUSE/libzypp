/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_TARGET_PRIVATE_COMMITPACKAGEPRELOADER_H
#define ZYPP_TARGET_PRIVATE_COMMITPACKAGEPRELOADER_H

#include <zypp-core/Pathname.h>
#include <zypp-core/ng/base/zyppglobal.h>
#include <zypp/sat/Transaction.h>
#include <zypp/media/UrlResolverPlugin.h>
#include <zypp/media/detail/DownloadProgressTracker.h>
#include <zypp/ZYppCallbacks.h>

#include <map>
#include <deque>
#include <chrono>

namespace zyppng {
ZYPP_FWD_DECL_TYPE_WITH_REFS(NetworkRequestDispatcher);
ZYPP_FWD_DECL_TYPE_WITH_REFS(NetworkRequest);
}

namespace zypp {

class CommitPackagePreloader
{
  using clock = std::chrono::steady_clock;
public:
  CommitPackagePreloader();

  void preloadTransaction( const std::vector<sat::Transaction::Step> &steps );
  void cleanupCaches();
  bool missed() const;

private:
  class PreloadWorker;
  struct RepoUrl {
    zypp::Url baseUrl;
    media::UrlResolverPlugin::HeaderList headers;
    int refs = 0; //< how many workers operate on the mirror
    int miss = 0; //< how many times this mirror had files misses
  };

  struct RepoDownloadData {
    std::vector<RepoUrl> _baseUrls;
  };

  void reportBytesDownloaded ( ByteCount newBytes );

  std::map<Repository::IdType, RepoDownloadData> _dlRepoInfo;
  std::deque<PoolItem> _requiredDls;
  std::vector<zyppng::Ref<PreloadWorker>> _workers;
  ByteCount _requiredBytes;
  ByteCount _downloadedBytes;
  bool _missedDownloads = false;

  callback::SendReport<media::CommitPreloadReport> _report;
  zyppng::Ref<internal::ProgressTracker> _pTracker;
  std::optional<clock::time_point> _lastProgressUpdate;

  zyppng::NetworkRequestDispatcherRef _dispatcher;
};

}

#endif // ZYPP_TARGET_PRIVATE_COMMITPACKAGEPRELOADER_H
