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
#ifndef ZYPP_MEDIA_PRIVATE_PROVIDE_ITEM_P_H_INCLUDED
#define ZYPP_MEDIA_PRIVATE_PROVIDE_ITEM_P_H_INCLUDED

#include "providefwd_p.h"
#include "providequeue_p.h"
#include "attachedmediainfo_p.h"
#include "providemessage_p.h"
#include <zypp-core/ng/async/iotask.h>
#include <zypp-media/ng/Provide>
#include <zypp-media/ng/ProvideItem>
#include <zypp-media/ng/ProvideRes>
#include <zypp-media/ng/ProvideSpec>
#include <zypp-core/ng/base/private/base_p.h>
#include <set>
#include <variant>

namespace zyppng {

  /*!
   * The internal request type, which represents all possible
   * user requests and exports some convenience functions for the scheduler to
   * directly access relevant data
   */
  class ProvideRequest {
  public:

    friend class ProvideItem;

    static expected<ProvideRequestRef> create( ProvideItem &owner, const zypp::MirroredOrigin &origin, const std::string &id, ProvideMediaSpec &spec );
    static expected<ProvideRequestRef> create ( ProvideItem &owner, const zypp::MirroredOrigin &origin, ProvideFileSpec &spec );
    static expected<ProvideRequestRef> createDetach( const zypp::Url &url );

    ProvideItem * owner() { return _owner; }

    uint code () const { return _message.code(); }

    void setCurrentQueue ( ProvideQueueRef ref );
    ProvideQueueRef currentQueue ();

    const ProvideMessage &provideMessage () const { return _message; }
    ProvideMessage &provideMessage () { return _message; }

    /**
     * Returns the currenty active URL as set by the scheduler
     */
    const std::optional<zypp::Url> activeUrl() const;
    void setActiveUrl ( const zypp::Url &urlToUse );

    void setOrigin( zypp::MirroredOrigin origin ) {
      _origin = std::move(origin);
    }

    const zypp::MirroredOrigin &origin() const {
      return _origin;
    }

    zypp::Url url() const {
      return _origin.authority().url();
    }

    void setUrl( const zypp::Url & url ) {
      _origin.setAuthority (url);
      _origin.clearMirrors ();
    }

    void clearForRestart () {
      _pastRedirects.clear();
      _activeUrl.reset();
      _myQueue.reset();
    }

  private:
    ProvideRequest( ProvideItem *owner, zypp::MirroredOrigin origin, ProvideMessage &&msg ) : _owner(owner), _message(std::move(msg) ), _origin ( std::move(origin) ) {}
    ProvideItem *_owner = nullptr; // destructor of ProvideItem will dequeue the item, so no need to do refcount here
    ProvideMessage _message;
    zypp::MirroredOrigin     _origin;
    std::vector<zypp::Url>   _pastRedirects;
    std::optional<zypp::Url> _activeUrl;
    ProvideQueueWeakRef _myQueue;
  };

  class ProvideItemPrivate : public BasePrivate
  {
    public:
      ProvideItemPrivate( ProvidePrivate & parent, ProvideItem &pub ) : BasePrivate(pub), _parent(parent) {}
      ProvidePrivate &_parent;
      ProvideItem::State _itemState = ProvideItem::Uninitialized;
      std::chrono::steady_clock::time_point _itemStarted;
      std::chrono::steady_clock::time_point _itemFinished;
      std::optional<ProvideItem::ItemStats> _prevStats;
      std::optional<ProvideItem::ItemStats> _currStats;
      Signal<void( ProvideItem &item, ProvideItem::State oldState, ProvideItem::State newState )> _sigStateChanged;
  };

  /*!
   * Item downloading and providing a file
   */
  class ProvideFileItem : public ProvideItem
  {
  public:

    static ProvideFileItemRef create (IOTaskAwaiter<expected<ProvideRes>> &promise, zypp::MirroredOrigin origin, const ProvideFileSpec &request, ProvidePrivate &parent );

    // ProvideItem interface
    void initialize () override;

    void setMediaRef ( Provide::MediaHandle &&hdl );
    Provide::MediaHandle & mediaRef ();

    ItemStats makeStats () override;
    zypp::ByteCount bytesExpected () const override;

  protected:
    ProvideFileItem (IOTaskAwaiter<expected<ProvideRes> > &promise, zypp::MirroredOrigin origin, const ProvideFileSpec &request, ProvidePrivate &parent );

    void informalMessage ( ProvideQueue &, ProvideRequestRef req, const ProvideMessage &msg  ) override;

    using ProvideItem::finishReq;
    void finishReq ( ProvideQueue &queue, ProvideRequestRef finishedReq, const ProvideMessage &msg ) override;
    void cancelWithError ( std::exception_ptr error ) override;
    expected<zypp::media::AuthData> authenticationRequired ( ProvideQueue &queue, ProvideRequestRef req, const zypp::Url &effectiveUrl, int64_t lastTimestamp, const std::map<std::string, std::string> &extraFields ) override;

  private:
    Provide::MediaHandle _handleRef;    //< If we are using a attached media, this will keep the reference around
    zypp::MirroredOrigin _origin;       //< All available URLs
    ProvideFileSpec     _initialSpec;   //< The initial spec as defined by the user code
    zypp::Pathname      _targetFile;    //< The target file as reported by the worker
    zypp::Pathname      _stagingFile;   //< The staging file as reported by the worker
    zypp::ByteCount     _expectedBytes; //< The nr of bytes we want to provide
    IOTaskAwaiter<expected<ProvideRes>> *_promise = nullptr;
  };


  /*!
   * Item attaching and verifying a medium
   */
  class AttachMediaItem : public ProvideItem
  {
  public:
    ~AttachMediaItem() override;
    static AttachMediaItemRef create ( IOTaskAwaiter<expected<Provide::MediaHandle>> &promise, const zypp::MirroredOrigin &origin, const ProvideMediaSpec &request, ProvidePrivate &parent );
    SignalProxy< void( const zyppng::expected<AttachedMediaInfo *> & ) > sigReady ();
  protected:
    AttachMediaItem ( IOTaskAwaiter<expected<Provide::MediaHandle>> &promise, const zypp::MirroredOrigin &origin, const ProvideMediaSpec &request, ProvidePrivate &parent );

    // ProvideItem interface
    void initialize () override;

    using ProvideItem::finishReq;
    void finishReq (  ProvideQueue &queue, ProvideRequestRef finishedReq, const ProvideMessage &msg ) override;
    void cancelWithError( std::exception_ptr error ) override;
    void finishWithSuccess (AttachedMediaInfo_Ptr medium );
    expected<zypp::media::AuthData> authenticationRequired ( ProvideQueue &queue, ProvideRequestRef req, const zypp::Url &effectiveUrl, int64_t lastTimestamp, const std::map<std::string, std::string> &extraFields ) override;

    void onMasterItemReady ( const zyppng::expected<AttachedMediaInfo *>& result );

  private:
    Signal< void( const zyppng::expected<AttachedMediaInfo *> & )> _sigReady;
    connection _masterItemConn;
    zypp::MirroredOrigin _origin;   //< All available URLs
    ProvideMediaSpec    _initialSpec;    //< The initial spec as defined by the user code
    ProvideQueue::Config::WorkerType _workerType = ProvideQueue::Config::Invalid;
    IOTaskAwaiter<expected<Provide::MediaHandle>> * _promise = nullptr;
    MediaDataVerifierRef _verifier;
  };
}

#endif
