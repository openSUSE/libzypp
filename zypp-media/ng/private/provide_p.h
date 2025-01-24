/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPP_MEDIA_PRIVATE_PROVIDE_P_H_INCLUDED
#define ZYPP_MEDIA_PRIVATE_PROVIDE_P_H_INCLUDED

#include "providefwd_p.h"
#include "providequeue_p.h"
#include "attachedmediainfo_p.h"

#include <zypp-media/ng/auth/credentialmanager.h>
#include <zypp-media/ng/Provide>
#include <zypp-media/ng/ProvideItem>
#include <zypp-media/ng/ProvideSpec>
#include <zypp-core/zyppng/base/private/base_p.h>
#include <zypp-core/zyppng/base/Timer>
#include <zypp-core/ManagedFile.h>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS ( InputRequest );

  namespace constants {
    constexpr std::string_view DEFAULT_PROVIDE_WORKER_PATH = ZYPP_WORKER_PATH;
    constexpr std::string_view ATTACHED_MEDIA_SUFFIX = "-media";
    constexpr auto DEFAULT_ACTIVE_CONN_PER_HOST = 5;   //< how many simultanious connections to the same host are allowed
    constexpr auto DEFAULT_ACTIVE_CONN          = 10;  //< how many simultanious connections are allowed
    constexpr auto DEFAULT_MAX_DYNAMIC_WORKERS  = 20;
    constexpr auto DEFAULT_CPU_WORKERS          = 4;
  }

  class ProvideQueue;
  class ProvidePrivate : public BasePrivate
  {
    ZYPP_DECLARE_PUBLIC(Provide)
  public:
    ProvidePrivate( zypp::Pathname &&workDir, Provide &pub );

    enum ScheduleReason
    {
      ProvideStart,
      QueueIdle,
      EnqueueItem,
      EnqueueReq,
      RestartAttach,
      FinishReq
    };

    void schedule( ScheduleReason reason );

    bool queueRequest  ( ProvideRequestRef req );
    bool dequeueRequest( ProvideRequestRef req, std::exception_ptr error );
    void queueItem     ( ProvideItemRef item );
    void dequeueItem   ( ProvideItem *item );


    using AuthRequestCb = std::function<void ( expected<zypp::media::AuthData> )>;
    void authenticationRequired ( ProgressObserverRef pO, ProvideRequestRef item, zypp::Url effectiveUrl, std::string username, std::map<std::string, std::string> extra, AuthRequestCb asyncReadyCb );
    void requestDestructing( ProvideRequest &req );

    std::string nextMediaId () const;
    AttachedMediaInfo_Ptr addMedium ( AttachedMediaInfo_Ptr &&medium );

    std::string effectiveScheme ( const std::string &scheme ) const;

    void onPulseTimeout ( Timer & );
    void onQueueIdle ();
    void onItemStateChanged ( ProvideItem &item );
    expected<ProvideQueue::Config> schemeConfig(const std::string &scheme);

    std::optional<zypp::ManagedFile> addToFileCache ( const zypp::Pathname &downloadedFile );
    bool isInCache ( const zypp::Pathname &downloadedFile ) const;

    bool isRunning() const;

    const zypp::Pathname &workerPath() const;
    const std::string queueName( ProvideQueue &q ) const;

    std::vector<AttachedMediaInfo_Ptr> &attachedMediaInfos();

    std::list<ProvideItemRef> &items();

    zypp::media::CredManagerSettings &credManagerOptions ();

    std::vector<zypp::Url> sanitizeUrls ( const std::vector<zypp::Url> &urls );

    ProvideStatusRef log () {
      return _log;
    }

    uint32_t nextRequestId();

  protected:
    void doSchedule (Timer &);

    //@TODO should we make those configurable?
    std::unordered_map< std::string, std::string > _workerAlias {
      {"ftp"  ,"http"},
      {"tftp" ,"http"},
      {"https","http"},
      {"cifs" ,"smb" },
      {"nfs4" ,"nfs" },
      {"cd"   ,"disc"},
      {"dvd"  ,"disc"},
      {"file" ,"dir" },
      {"hd"   ,"disk"}
    };

    bool _isRunning = false;
    bool _isScheduling = false;
    Timer::Ptr _pulseTimer = Timer::create();
    Timer::Ptr _scheduleTrigger = Timer::create(); //< instead of constantly calling schedule we set a trigger event so it runs as soon as event loop is on again
    zypp::Pathname _workDir;

    std::list< ProvideItemRef > _items; //< The list of running provide Items, each of them can spawn multiple requests
    uint32_t _nextRequestId = 0; //< The next request ID , we use controller wide unique IDs instead of worker locals IDs , its easier to track

    struct QueueItem {
      std::string _schemeName;
      std::deque<ProvideRequestRef> _requests;
    };
    std::deque<QueueItem> _queues; //< List of request queues for the workers, grouped by scheme. We use a deque and not a map because of possible changes to the list of queues during scheduling

    struct AuthRequest {
      zypp::Url   _url;
      std::string _username;
      InputRequestRef _userRequest;

      struct Waiter {
        ProvideRequest *_reqRef; // ProvideRequest will unregister all of its Auth events on destructions
        AuthRequestCb _asyncReadyCb;
      };
      std::vector<Waiter> _authWaiters;
    };
    std::vector<AuthRequest> _authRequests; //< List of all Authentication requests, grouped by URL and username.. we try to coalesce the requests if possible


    std::vector< AttachedMediaInfo_Ptr > _attachedMediaInfos; //< List of currently attached medias

    std::unordered_map< std::string, ProvideQueueRef > _workerQueues;
    std::unordered_map< std::string, ProvideQueue::Config > _schemeConfigs;

    struct FileCacheItem {
      zypp::ManagedFile _file;
      std::optional<std::chrono::steady_clock::time_point> _deathTimer; // timepoint where this item was seen first without a refcount
    };
    std::unordered_map< std::string, FileCacheItem > _fileCache;

    zypp::Pathname _workerPath;
    zypp::media::CredManagerSettings _credManagerOptions;

    ProvideStatusRef _log;
    Signal<void()> _sigIdle;
  };
}

#endif
