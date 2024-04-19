/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "mediafacade.h"
#include "zypp/media/MediaHandlerFactory.h"
#include <zypp/ZYppCallbacks.h>
#include <zypp-core/TriBool.h>
#include <zypp-core/base/userrequestexception.h>
#include <utility>
#include <zypp-media/ng/ProvideSpec>
#include <zypp-media/mount.h>
#include <zypp/repo/SUSEMediaVerifier.h>

namespace zyppng {

  class AttachedSyncMediaInfo  : public zypp::base::ReferenceCounted, private zypp::base::NonCopyable
  {

  public:

    AttachedSyncMediaInfo( MediaSyncFacadeRef parentRef, zypp::media::MediaAccessId mediaId, zypp::Url baseUrl, ProvideMediaSpec mediaSpec, const zypp::Pathname &locPath );

    zypp::media::MediaAccessId mediaId () const;
    const ProvideMediaSpec &spec() const;
    const zypp::Url &url() const;
    const std::optional<zypp::Pathname> &rootPath() const;
    MediaSyncFacadeRef parent() const;

    /*!
     * Returns true if \a other requests the same medium as this instance
     */
    bool isSameMedium ( const std::vector<zypp::Url> &urls, const ProvideMediaSpec &spec );

    // ReferenceCounted interface
  protected:
    void unref_to(unsigned int) const override;

  private:
    zypp::media::MediaAccessId _id;
    zypp::Url _attachedUrl;
    ProvideMediaSpec _spec;
    MediaSyncFacadeRef _parent;
    std::optional<zypp::Pathname> _localPath;
  };

  IMPL_PTR_TYPE( AttachedSyncMediaInfo );

  AttachedSyncMediaInfo::AttachedSyncMediaInfo(MediaSyncFacadeRef parentRef, zypp::media::MediaAccessId mediaId, zypp::Url baseUrl, ProvideMediaSpec mediaSpec, const zypp::Pathname &locPath)
    : _id( mediaId )
    , _attachedUrl(std::move( baseUrl ))
    , _spec(std::move( mediaSpec ))
    , _parent(std::move( parentRef ))
    , _localPath( locPath )
  {}

  zypp::media::MediaAccessId AttachedSyncMediaInfo::mediaId() const
  {
    return _id;
  }

  const ProvideMediaSpec &AttachedSyncMediaInfo::spec() const
  {
    return _spec;
  }

  const zypp::Url &AttachedSyncMediaInfo::url() const
  {
    return _attachedUrl;
  }

  const std::optional<zypp::Pathname> &AttachedSyncMediaInfo::rootPath() const
  {
    return _localPath;
  }

  MediaSyncFacadeRef AttachedSyncMediaInfo::parent() const
  {
    return _parent;
  }

  bool AttachedSyncMediaInfo::isSameMedium(const std::vector<zypp::Url> &urls, const ProvideMediaSpec &spec) {

    const auto check = _spec.isSameMedium(spec);
    if ( !zypp::indeterminate (check) )
      return (bool)check;

    // let the URL rule
    return ( std::find( urls.begin(), urls.end(), _attachedUrl ) != urls.end() );
  }

  void AttachedSyncMediaInfo::unref_to( unsigned int count ) const
  {
    // once count reaches 1 only the MediaSyncFacade holds a reference,
    // time to release the medium
    if ( count == 1 ) {
      _parent->releaseMedium ( this );
      // !!!! careful from here on out 'this' is most likely invalid  !!!!
      return;
    }
  }


  SyncMediaHandle::SyncMediaHandle(){ }

  SyncMediaHandle::SyncMediaHandle(AttachedSyncMediaInfo_Ptr dataPtr) : _data( std::move(dataPtr) )
  { }

  MediaSyncFacadeRef SyncMediaHandle::parent() const
  {
    return _data->parent();
  }

  bool SyncMediaHandle::isValid() const
  {
    return _data.get() != nullptr;
  }

  const zypp::Url &SyncMediaHandle::baseUrl() const
  {
    static zypp::Url invalidHandle;
    if ( !_data )
      return invalidHandle;
    return _data->url();
  }

  const std::optional<zypp::Pathname> &SyncMediaHandle::localPath() const
  {
    static std::optional<zypp::Pathname> invalidPath;
    if ( !_data )
      return invalidPath;
    return _data->rootPath();
  }

  const AttachedSyncMediaInfo &SyncMediaHandle::info() const
  {
    return *_data;
  }

  MediaSyncFacade::Res::Res(MediaHandle hdl, zypp::ManagedFile file)
    : _res( std::move(file) )
    , _provideHandle( std::move (hdl) )
  { }

  const zypp::Pathname MediaSyncFacade::Res::file() const {
    return _res;
  }

  ZYPP_IMPL_PRIVATE_CONSTR (MediaSyncFacade)  {  }

  std::vector<zypp::Url> MediaSyncFacade::sanitizeUrls(const std::vector<zypp::Url> &urls) const
  {
    std::vector<zypp::Url> usableMirrs;
    std::optional<zypp::media::MediaHandlerFactory::MediaHandlerType> handlerType;

    for ( auto mirrIt = urls.begin() ; mirrIt != urls.end(); mirrIt++ ) {
      const auto &s = zypp::media::MediaHandlerFactory::handlerType ( *mirrIt );
      if ( !s ) {
        WAR << "URL: " << *mirrIt << " is not supported, ignoring!" << std::endl;
        continue;
      }
      if ( !handlerType ) {
        handlerType = *s;
        usableMirrs.push_back ( *mirrIt );
      } else {
        if ( handlerType == *s) {
          usableMirrs.push_back( *mirrIt );
        } else {
          WAR << "URL: " << *mirrIt << " has different handler type than the primary URL: "<< usableMirrs.front() <<", ignoring!" << std::endl;
        }
      }
    }

    if ( !handlerType || usableMirrs.empty() ) {
      return {};
    }

    return usableMirrs;
  }

  expected<MediaSyncFacade::MediaHandle> MediaSyncFacade::attachMedia( const zypp::Url &url, const ProvideMediaSpec &request )
  {

    bool isVolatile = url.schemeIsVolatile();

    auto effectiveUrl = url;

    std::optional<zypp::media::MediaAccessId> attachId;
    zypp::callback::SendReport<zypp::media::MediaChangeReport> report;

    // nothing attached, make a new one
    zypp::media::MediaManager mgr;
    do {
      try {
        if ( !attachId ) {

          if ( request.medianr() > 1 )
            effectiveUrl = zypp::MediaSetAccess::rewriteUrl( effectiveUrl, request.medianr() );

          attachId = mgr.open( effectiveUrl );
          if ( !request.mediaFile().empty() ) {
            mgr.addVerifier( *attachId, zypp::media::MediaVerifierRef( new zypp::repo::SUSEMediaVerifier( request.mediaFile(), request.medianr() ) ) );
          }
        }

        // attach the medium
        mgr.attach( *attachId );

        auto locPath = mgr.localPath( *attachId, "/" );
        auto attachInfo = AttachedSyncMediaInfo_Ptr( new AttachedSyncMediaInfo( shared_this<MediaSyncFacade>(), *attachId, url, request, locPath )  );
        _attachedMedia.push_back( attachInfo );
        return expected<MediaSyncFacade::MediaHandle>::success( std::move(attachInfo) );

      } catch ( const zypp::media::MediaException &excp ) {

        ZYPP_CAUGHT(excp);

        // if no one is listening, just return the error as is
        if ( !zypp::callback::SendReport<zypp::media::MediaChangeReport>::connected() || !attachId ) {
          return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_FWD_CURRENT_EXCPT() );
        }

        // default action is to cancel
        zypp::media::MediaChangeReport::Action user = zypp::media::MediaChangeReport::ABORT;

        do {
          // here: Manager tried all not attached drives and could not find the desired medium
          // we need to send the media change report

          // set up the reason
          auto reason = zypp::media::MediaChangeReport::INVALID;
          if( typeid(excp) == typeid( zypp::media::MediaNotDesiredException) ) {
            reason = zypp::media::MediaChangeReport::WRONG;
          }

          unsigned int devindex = 0;

          std::vector<std::string> devices;
          mgr.getDetectedDevices(*attachId, devices, devindex);

          std::optional<std::string> currentlyUsed;
          if ( devices.size() ) currentlyUsed = devices[devindex];

          if ( isVolatile ) {
            // filter devices that are mounted, aka used, we can not eject them
            const auto &mountedDevs = zypp::media::Mount::getEntries();
            std::remove_if( devices.begin (), devices.end(), [&](const std::string &dev) {
              zypp::PathInfo devInfo(dev);
              return std::any_of( mountedDevs.begin (), mountedDevs.end(), [&devInfo]( const zypp::media::MountEntry &e ) {
                zypp::PathInfo pi( e.src );
                return ( pi.isBlk() && pi.devMajor() == devInfo.devMajor() && pi.devMinor() == devInfo.devMinor() );
              });
            });

            if ( !devices.size () ) {
              // Jammed, no currently free device
              MIL << "No free device available, return jammed and try again later ( hopefully) " << std::endl;
              if ( attachId ) mgr.close ( *attachId );
              return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_EXCPT_PTR(zypp::media::MediaJammedException()) );
            }

            // update index to currenty used dev
            bool foundCurrent = false;
            if ( currentlyUsed ) {
              for ( unsigned int i = 0; i < devices.size(); i++ ) {
                if ( devices[i] == *currentlyUsed ) {
                  foundCurrent = true;
                  devindex = i;
                  break;
                }
              }
            }

            if ( !foundCurrent ){
              devindex = 0; // seems 0 is what is set in the handlers too if there is no current
            }
          }

          user = report->requestMedia (
            effectiveUrl,
            request.medianr(),
            request.label(),
            reason,
            excp.asUserHistory(),
            devices,
            devindex
          );

          MIL << "ProvideFile exception caught, callback answer: " << user << std::endl;

          switch ( user ) {
            case zypp::media::MediaChangeReport::ABORT: {
              DBG << "Aborting" << std::endl;
              if ( attachId ) mgr.close ( *attachId );
              zypp::AbortRequestException aexcp("Aborting requested by user");
              aexcp.remember(excp);
              return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_EXCPT_PTR(aexcp) );
            }
            case zypp::media::MediaChangeReport::IGNORE: {
              DBG << "Skipping" << std::endl;
              if ( attachId ) mgr.close ( *attachId );
              zypp::SkipRequestException nexcp("User-requested skipping of a file");
              nexcp.remember(excp);
              return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_EXCPT_PTR(nexcp) );
            }
            case zypp::media::MediaChangeReport::EJECT: {
              DBG << "Eject: try to release" << std::endl;
              try
              {
                // MediaSetAccess does a releaseAll, but we can not release other devices that are in use
                // media_mgr.releaseAll();
                mgr.release (*attachId, devindex < devices.size() ? devices[devindex] : "");
              }
              catch ( const zypp::Exception & e)
              {
                ZYPP_CAUGHT(e);
              }
              break;
            }
            case zypp::media::MediaChangeReport::RETRY:
            case zypp::media::MediaChangeReport::CHANGE_URL: {
              // retry
              DBG << "Going to try again" << std::endl;

              // invalidate current media access id
              if ( attachId ) {
                mgr.close(*attachId);
                attachId.reset();
              }

              // not attaching, media set will do that for us
              // this could generate uncaught exception (#158620)
              break;
            }
            default: {
              DBG << "Don't know, let's ABORT" << std::endl;
              if ( attachId ) mgr.close ( *attachId );
              return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_FWD_CURRENT_EXCPT() );
            }
          }
        } while( user == zypp::media::MediaChangeReport::EJECT );
      } catch ( const zypp::Exception &e ) {
        ZYPP_CAUGHT(e);
        if ( attachId ) mgr.close ( *attachId );
        return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_FWD_CURRENT_EXCPT() );
      } catch (...) {
        // didn't work -> clean up
        if ( attachId ) mgr.close ( *attachId );
        return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_FWD_CURRENT_EXCPT() );
      }
    } while ( true );
  }

  expected<MediaSyncFacade::MediaHandle> MediaSyncFacade::attachMedia( const std::vector<zypp::Url> &urls, const ProvideMediaSpec &request )
  {
    // rewrite and sanitize the urls if required
    std::vector<zypp::Url> useableUrls = sanitizeUrls(urls);

    if ( useableUrls.empty () )
      return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_EXCPT_PTR ( zypp::media::MediaException("No valid mirrors available") ));

    // first try and find a already attached medium
    auto i = std::find_if( _attachedMedia.begin (), _attachedMedia.end(), [&]( const AttachedSyncMediaInfo_Ptr &medium ) {
      return medium->isSameMedium( useableUrls, request );
    });

    if ( i != _attachedMedia.end() ) {
      return expected<MediaSyncFacade::MediaHandle>::success( *i );
    }


    std::exception_ptr lastError;
    std::exception_ptr jammedError;


    // from here call attachMedia with just one URL
    // catch errors, if one of the URLS returns JAMMED, remember it
    // continue to try other URLs, if they all fail and JAMMED was remembered
    // return it, otherwise the last error

    for ( const auto &url : useableUrls ) {
      try {
        return expected<MediaSyncFacade::MediaHandle>::success( attachMedia( url, request ).unwrap() );
      } catch ( zypp::media::MediaJammedException &e) {
        ZYPP_CAUGHT(e);
        if ( !jammedError ) jammedError = std::current_exception ();
      }  catch ( const zypp::Exception &e ) {
        ZYPP_CAUGHT(e);
        lastError = std::current_exception();
      } catch (...) {
        // didn't work -> clean up
        lastError = std::current_exception();
      }
    }

    if ( jammedError ) {
      // if we encountered a jammed error return it, there might be still a chance
      // that invoking the provide again after other pipelines have finished might succeed
      return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_FWD_EXCPT(jammedError) );
    } else if ( lastError ) {
      // if we have a error stored, return that one
      return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_FWD_EXCPT(lastError) );
    }

    return expected<MediaSyncFacade::MediaHandle>::error( ZYPP_EXCPT_PTR( zypp::media::MediaException("No URL to attach") ) );
  }

  zyppng::MediaSyncFacade::~MediaSyncFacade()
  {
    // this should never happen because every handle has a reference to the media manager, but still add a debug output
    // so we know in case we have weird behavior.
    if ( _attachedMedia.size () ) {
      WAR << "Releasing zyppng::MediaSyncFacade with still valid MediaHandles, this is a bug!" << std::endl;
    }
  }

  expected<MediaSyncFacade::LazyMediaHandle> MediaSyncFacade::prepareMedia(const std::vector<zypp::Url> &urls, const ProvideMediaSpec &request)
  {
    const auto &useableUrls = sanitizeUrls(urls);
    if ( useableUrls.empty () )
      return expected<MediaSyncFacade::LazyMediaHandle>::error( ZYPP_EXCPT_PTR ( zypp::media::MediaException("No valid mirrors available") ));
    return expected<LazyMediaHandle>::success( shared_this<MediaSyncFacade>(), std::move(useableUrls), request );
  }

  expected<MediaSyncFacade::LazyMediaHandle> MediaSyncFacade::prepareMedia(const zypp::Url &url, const ProvideMediaSpec &request)
  {
    return prepareMedia( std::vector<zypp::Url>{url}, request );
  }

  expected<MediaSyncFacade::MediaHandle> MediaSyncFacade::attachMediaIfNeeded( LazyMediaHandle lazyHandle)
  {
    using namespace zyppng::operators;
    if ( lazyHandle.attached() )
      return expected<MediaHandle>::success( *lazyHandle.handle() );

    MIL << "Attaching lazy medium with label: [" << lazyHandle.spec().label() << "]" << std::endl;

    return attachMedia( lazyHandle.urls(), lazyHandle.spec () )
        | and_then([lazyHandle]( MediaHandle handle ) {
          lazyHandle._sharedData->_mediaHandle = handle;
          return expected<MediaHandle>::success( std::move(handle) );
        });
  }

  expected<MediaSyncFacade::Res> MediaSyncFacade::provide(const std::vector<zypp::Url> &urls, const ProvideFileSpec &request)
  {
    using namespace zyppng::operators;

    if ( !urls.size() )
      return expected<MediaSyncFacade::Res>::error( ZYPP_EXCPT_PTR ( zypp::media::MediaException("Can not provide a file without a URL.") ));

    std::optional<expected<MediaSyncFacade::Res>> lastErr;
    for ( const zypp::Url& file_url : urls ) {

      zypp::Url url(file_url);
      zypp::Pathname fileName(url.getPathName());
      url.setPathName ("/");

      expected<MediaSyncFacade::Res> res = attachMedia( url, ProvideMediaSpec( "" ) )
          | and_then( [&, this]( const MediaSyncFacade::MediaHandle& handle ) {
              return provide( handle, fileName, request.asOnMediaLocation(fileName, 1));
            });

      if ( res )
        return res;

      lastErr = res;
    }

    // we always should have a last error, except if the URLs are empty
    if ( lastErr )
      return *lastErr;

    // we should not get here, but if we do simply use the first entry to make a not found error
    zypp::Url url( urls.front() );
    zypp::Pathname fileName(url.getPathName());
    url.setPathName ("/");
    return expected<MediaSyncFacade::Res>::error( ZYPP_EXCPT_PTR ( zypp::media::MediaFileNotFoundException( url, fileName )));

  }

  expected<MediaSyncFacade::Res> MediaSyncFacade::provide(const zypp::Url &url, const ProvideFileSpec &request)
  {
    return provide( std::vector<zypp::Url>{url}, request );
  }

  expected<MediaSyncFacade::Res> MediaSyncFacade::provide(const MediaHandle &attachHandle, const zypp::Pathname &fileName, const ProvideFileSpec &request)
  {
    zypp::media::MediaManager mgr;
    const auto &handleInfo = attachHandle.info();

    try {
      if ( request.checkExistsOnly() ) {
        if ( !mgr.doesFileExist ( handleInfo.mediaId (), fileName ) ) {
          return expected<MediaSyncFacade::Res>::error( ZYPP_EXCPT_PTR( zypp::media::MediaFileNotFoundException( handleInfo.url(), fileName ) ) );
        }

        // we return a result pointing to a non existant file, since the code just asked us to check if the file exists
        return expected<MediaSyncFacade::Res>::success( attachHandle, zypp::ManagedFile( mgr.localPath( handleInfo.mediaId(), fileName ) ) );

      } else {
        mgr.provideFile( handleInfo.mediaId (), request.asOnMediaLocation( fileName, handleInfo.spec().medianr()) );

        zypp::ManagedFile locFile( mgr.localPath( handleInfo.mediaId(), fileName ) );

       // do not clean up files for now, they are cleaned up anyways on detach
#if 0
        // if the file is downloaded we want to clean it up again
        if ( handleInfo.url().schemeIsDownloading() )
          locFile.setDispose ( zypp::filesystem::unlink );
#endif

        return expected<MediaSyncFacade::Res>::success( attachHandle, locFile );
      }
    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT(e);
      return expected<MediaSyncFacade::Res>::error(ZYPP_FWD_CURRENT_EXCPT());
    } catch (...) {
      return expected<MediaSyncFacade::Res>::error(ZYPP_FWD_CURRENT_EXCPT());
    }
  }

  expected<MediaSyncFacade::Res> MediaSyncFacade::provide( const LazyMediaHandle &attachHandle, const zypp::Pathname &fileName, const ProvideFileSpec &request )
  {
    using namespace zyppng::operators;
    return attachMediaIfNeeded ( attachHandle )
    | and_then([weakMe = weak_this<MediaSyncFacade>(), fName = fileName, req = request ]( MediaHandle handle ){
      auto me = weakMe.lock();
      if ( !me )
        return expected<Res>::error(ZYPP_EXCPT_PTR(zypp::Exception("Provide was released during a operation")));
      return me->provide( handle, fName, req);
    });
  }

  zyppng::expected<zypp::CheckSum> MediaSyncFacade::checksumForFile(const zypp::Pathname &p, const std::string &algorithm)
  {
    try {
      return expected<zypp::CheckSum>::success( zypp::CheckSum( algorithm, zypp::filesystem::checksum ( p, algorithm ) ) );
    } catch(...) {
      return expected<zypp::CheckSum>::error ( std::current_exception () );
    }
  }

  expected<zypp::ManagedFile> MediaSyncFacade::copyFile(const zypp::Pathname &source, const zypp::Pathname &target)
  {
    try {
      // do what Provide would do and make a URL
      zypp::Url url("copy:///");
      url.setPathName( source );

      auto sourcePi = zypp::PathInfo(source);
      if ( !sourcePi.isExist() ) {
        return expected<zypp::ManagedFile>::error ( ZYPP_EXCPT_PTR( zypp::media::MediaFileNotFoundException( url, "" ) ) );
      }
      if ( !sourcePi.isFile () )
        return expected<zypp::ManagedFile>::error ( ZYPP_EXCPT_PTR( zypp::media::MediaNotAFileException( url, "" ) ) );

      auto res = zypp::filesystem::hardlinkCopy( source, target.asString() );
      if ( res == 0 ) {
        return expected<zypp::ManagedFile>::success( zypp::ManagedFile( target, zypp::filesystem::unlink ) );
      } else {
        return expected<zypp::ManagedFile>::error ( ZYPP_EXCPT_PTR( zypp::media::MediaException( zypp::str::Str() << "Failed to create file " << target << " errno: " << res ) ) );
      }
    } catch(...) {
      return expected<zypp::ManagedFile>::error ( std::current_exception () );
    }
  }

  expected<zypp::ManagedFile> MediaSyncFacade::copyFile(zyppng::MediaSyncFacade::Res source, const zypp::Pathname &target)
  {
    // not much to do here, since this will block until the file has been copied we do not need to remember the ProvideRes
    return copyFile( source.file(), target );
  }

  void zyppng::MediaSyncFacade::releaseMedium(const AttachedSyncMediaInfo *ptr)
  {
    if ( !ptr ) return;

    auto i = std::find_if(_attachedMedia.begin (), _attachedMedia.end(), [&]( const auto &p ) { return p.get() == ptr; } );

    try {
      zypp::media::MediaManager mgr;
      mgr.close ( ptr->mediaId() );
    } catch ( const zypp::Exception & e ) {
      ZYPP_CAUGHT(e);
    }

    if ( i != _attachedMedia.end() ) {
      _attachedMedia.erase(i);
    } else {
      ERR << "Releasing unknown medium " << ptr->mediaId () << " should not happen";
    }
  }


}
