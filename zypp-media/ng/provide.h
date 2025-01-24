/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_MEDIA_PROVIDE_H_INCLUDED
#define ZYPP_MEDIA_PROVIDE_H_INCLUDED

#include <zypp-core/zyppng/ui/userrequest.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/ManagedFile.h>
#include <zypp-core/zyppng/base/zyppglobal.h>
#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/async/AsyncOp>
#include <zypp-core/zyppng/pipelines/expected.h>
#include <zypp-core/ByteCount.h>
#include <zypp-media/ng/LazyMediaHandle>
#include <zypp-media/ng/ProvideFwd>
#include <zypp-media/ng/ProvideRes>
#include <zypp-media/auth/AuthData>
#include <boost/any.hpp>

namespace zypp {
  class Url;
  namespace media {
    struct CredManagerSettings;
  }
}

/*!
 * @TODO Fix bsc#1174011 "auth=basic ignored in some cases" for provider
 *       We should proactively add the password to the request if basic auth is configured
 *       and a password is available in the credentials but not in the URL.
 *
 *       We should be a bit paranoid here and require that the URL has a user embedded, otherwise we go the default route
 *       and ask the server first about the auth method
 *
 * @TODO Make sure URLs are rewritten, e.g. MediaSetAccess::rewriteUrl
 */
namespace zyppng {


  /*!
   * A media user request asking the user to provide authentication
   * for a URL.
   *
   * RequestType: InputRequest
   * Userdata fields:
   * - "url"   Url in question
   * - "username" last Username given to zypp
   */
  namespace ProvideAuthRequest {
    constexpr std::string_view CTYPE ("media/auth-request");
    UserData makeData ( const zypp::Url &url, const std::string &username );
  }

  /*!
   * A media user request asking the user to provide a different medium
   * than the currently available ones.
   *
   * RequestType: InputRequest
   * Userdata fields:
   * - "label"   Label of the required medium
   * - "mediaNr" Media Nr of the required medium
   * - "freeDevs" list of free devices the user can utilize to provide the medium
   * - "desc"    Media description (optional)
   */
  namespace ProvideMediaChangeRequest {
    constexpr std::string_view CTYPE ("media/media-change-request");
    UserData makeData( const std::string &label, const int32_t mediaNr, const std::vector<std::string> &freeDevs, const std::optional<std::string> &desc );
  }


  class ProvidePrivate;
  using AnyMap = std::unordered_map<std::string, boost::any>;
  DEFINE_PTR_TYPE(AttachedMediaInfo);

  ZYPP_FWD_DECL_TYPE_WITH_REFS (ProgressObserver);

  /*!
  * RAII helper for media handles
  */
  class ProvideMediaHandle
  {
    public:

      using ParentType = Provide;

      ProvideMediaHandle () = default;
      ProvideMediaHandle ( Provide &parent, AttachedMediaInfo_Ptr mediaInfoRef );
      std::shared_ptr<Provide> parent() const;
      bool isValid () const;
      std::string handle() const;
      const zypp::Url &baseUrl() const;
      const std::optional<zypp::Pathname> &localPath() const;
      std::optional<ProvideMediaSpec> spec() const;
      zyppng::AttachedMediaInfo_constPtr mediaInfo() const;
    private:
      ProvideWeakRef _parent;
      AttachedMediaInfo_Ptr _mediaRef;
  };

  /*!
   * Provide status observer object, this can be used to provide good insight into the status of the provider, its items and
   * all running requests.
   */
  class ProvideStatus
  {
    public:

      struct Stats {
        std::chrono::steady_clock::time_point _startTime;
        std::chrono::steady_clock::time_point _lastPulseTime;
        uint            _itemsSinceStart = 0; //< How many items have been started since Provide::start() was called
        uint            _runningItems    = 0; //< How many items are currently running
        zypp::ByteCount _finishedBytes; //< The number of bytes that were finished completely
        zypp::ByteCount _expectedBytes; //< The number of currently expected bytes
        zypp::ByteCount _partialBytes;  //< The number of bytes of items that were already partially downloaded but the item they belong to is not finished
        zypp::ByteCount _perSecondSinceLastPulse; //< The download speed since the last pulse
        zypp::ByteCount _perSecond;               //< The download speed we are currently operating with
      };

      ProvideStatus( ProvideRef parent );
      virtual ~ProvideStatus(){}

      virtual void provideStart ();
      virtual void provideDone  (){}
      virtual void itemStart    ( ProvideItem &item ){}
      virtual void itemDone     ( ProvideItem &item );
      virtual void itemFailed   ( ProvideItem &item );
      virtual void requestStart    ( ProvideItem &item, uint32_t reqId, const zypp::Url &url, const AnyMap &extraData = {} ){}
      virtual void requestDone     ( ProvideItem &item, uint32_t reqId, const AnyMap &extraData = {} ){}
      virtual void requestRedirect ( ProvideItem &item, uint32_t reqId, const zypp::Url &toUrl, const AnyMap &extraData = {} ){}
      virtual void requestFailed   ( ProvideItem &item, uint32_t reqId, const std::exception_ptr &err, const AnyMap &requestData = {} ){}
      virtual void pulse ( );

      const Stats &stats() const;

    private:
      Stats _stats;
      ProvideWeakRef _provider;
  };

  class Provide : public Base
  {
    ZYPP_DECLARE_PRIVATE(Provide)
    template<class T> friend class ProvidePromise;
    friend class ProvideItem;
    friend class ProvideMediaHandle;
    friend class ProvideStatus;
  public:

    using MediaHandle     = ProvideMediaHandle;
    using LazyMediaHandle = zyppng::LazyMediaHandle<Provide>;
    using Res = ProvideRes;

    static ProvideRef create( const zypp::Pathname &workDir = "" );

    /*!
     * Prepares a lazy handle, that is attached only if a actual provide() is called onto it.
     * Use this to delay a media attach until its used the first time
     */
    expected<LazyMediaHandle> prepareMedia ( const std::vector<zypp::Url> &urls, const ProvideMediaSpec &request );
    expected<LazyMediaHandle> prepareMedia ( const zypp::Url &url, const ProvideMediaSpec &request );

    AsyncOpRef<expected<MediaHandle>> attachMediaIfNeeded( LazyMediaHandle lazyHandle, ProgressObserverRef tracker = nullptr );
    AsyncOpRef<expected<MediaHandle>> attachMedia( const std::vector<zypp::Url> &urls, const ProvideMediaSpec &request, ProgressObserverRef tracker = nullptr );
    AsyncOpRef<expected<MediaHandle>> attachMedia( const zypp::Url &url, const ProvideMediaSpec &request, ProgressObserverRef tracker = nullptr );

    AsyncOpRef<expected<ProvideRes>> provide(  MediaContextRef ctx, const std::vector<zypp::Url> &urls, const ProvideFileSpec &request, ProgressObserverRef tracker = nullptr );
    AsyncOpRef<expected<ProvideRes>> provide(  MediaContextRef ctx, const zypp::Url &url, const ProvideFileSpec &request, ProgressObserverRef tracker = nullptr );
    AsyncOpRef<expected<ProvideRes>> provide(  const MediaHandle &attachHandle, const zypp::Pathname &fileName, const ProvideFileSpec &request, ProgressObserverRef tracker = nullptr );
    AsyncOpRef<expected<ProvideRes>> provide(  const LazyMediaHandle &attachHandle, const zypp::Pathname &fileName, const ProvideFileSpec &request, ProgressObserverRef tracker = nullptr );


    /*!
     * Schedules a job to calculate the checksum for the given file
     */
    AsyncOpRef<expected<zypp::CheckSum>> checksumForFile ( MediaContextRef ctx, const zypp::Pathname &p, const std::string &algorithm, ProgressObserverRef tracker = nullptr );

    /*!
     * Schedules a copy job to copy a file from \a source to \a target
     */
    AsyncOpRef<expected<zypp::ManagedFile>> copyFile ( MediaContextRef ctx, const zypp::Pathname &source, const zypp::Pathname &target, ProgressObserverRef tracker = nullptr );
    AsyncOpRef<expected<zypp::ManagedFile>> copyFile ( MediaContextRef ctx, ProvideRes &&source, const zypp::Pathname &target, ProgressObserverRef tracker = nullptr );

    void start();
    void setWorkerPath( const zypp::Pathname &path );
    bool isRunning() const;
    bool ejectDevice ( const std::string &queueRef, const std::string &device );

    void setStatusTracker( ProvideStatusRef tracker );

    const zypp::Pathname &providerWorkdir () const;

    const zypp::media::CredManagerSettings &credManangerOptions () const;
    void setCredManagerOptions(const zypp::media::CredManagerSettings &opt );

    SignalProxy<void()> sigIdle();

    static auto copyResultToDest ( ProvideRef provider, MediaContextRef ctx, zypp::Pathname targetPath, ProgressObserverRef tracker = nullptr ) {
      return [ providerRef=std::move(provider), opContext = std::move(ctx), targetPath = std::move(targetPath), tracker = std::move(tracker) ]( ProvideRes &&file ) mutable {
        zypp::filesystem::assert_dir( targetPath.dirname () );
        return providerRef->copyFile( std::move(opContext), std::move(file), std::move(targetPath), std::move(tracker) );
      };
    }

  private:
    Provide(  const zypp::Pathname &workDir );
  };

}
#endif
