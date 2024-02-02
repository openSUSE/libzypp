/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "mediafacade.h"
#include <zypp-core/TriBool.h>
#include <zypp-media/ng/ProvideSpec>
#include <zypp/repo/SUSEMediaVerifier.h>

namespace zyppng {

  class AttachedSyncMediaInfo  : public zypp::base::ReferenceCounted, private zypp::base::NonCopyable
  {

  public:

    AttachedSyncMediaInfo( MediaSyncFacadeRef parentRef, zypp::media::MediaAccessId mediaId, const zypp::Url &baseUrl, const ProvideMediaSpec &mediaSpec, const zypp::Pathname &locPath );

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

  AttachedSyncMediaInfo::AttachedSyncMediaInfo(MediaSyncFacadeRef parentRef, zypp::media::MediaAccessId mediaId, const zypp::Url &baseUrl, const ProvideMediaSpec &mediaSpec, const zypp::Pathname &locPath)
    : _id( mediaId )
    , _attachedUrl( baseUrl )
    , _spec( mediaSpec )
    , _parent( parentRef )
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

  expected<MediaSyncFacade::MediaHandle> MediaSyncFacade::attachMedia( const std::vector<zypp::Url> &urls, const ProvideMediaSpec &request )
  {
    // rewrite and sanitize the urls if required
    std::vector<zypp::Url> useableUrls = urls;

    // first try and find a already attached medium
    auto i = std::find_if( _attachedMedia.begin (), _attachedMedia.end(), [&]( const AttachedSyncMediaInfo_Ptr &medium ) {
      return medium->isSameMedium( useableUrls, request );
    });

    if ( i != _attachedMedia.end() ) {
      return expected<MediaSyncFacade::MediaHandle>::success( *i );
    }

    // nothing attached, make a new one
    zypp::media::MediaManager mgr;
    std::exception_ptr lastError;
    for ( const auto &url : useableUrls ) {
      std::optional<zypp::media::MediaAccessId> attachId;
      try {

        attachId = mgr.open( url );
        if ( !request.mediaFile().empty() ) {
          mgr.addVerifier( *attachId, zypp::media::MediaVerifierRef( new zypp::repo::SUSEMediaVerifier( request.mediaFile(), request.medianr() ) ) );
        }

        // attach the medium
        mgr.attach( *attachId );

        auto locPath = mgr.localPath( *attachId, "/" );
        auto attachInfo = AttachedSyncMediaInfo_Ptr( new AttachedSyncMediaInfo( shared_this<MediaSyncFacade>(), *attachId, url, request, locPath )  );
        _attachedMedia.push_back( attachInfo );
        return expected<MediaSyncFacade::MediaHandle>::success( std::move(attachInfo) );

      } catch ( const zypp::Exception &e ) {
        lastError = std::current_exception();
        ZYPP_CAUGHT(e);
      } catch (...) {
        // didn't work -> clean up
        lastError = std::current_exception();
      }

      // this URL wasn't the one, prepare to try a new one
      if ( attachId )
        mgr.close ( *attachId );

      attachId.reset();
    }

    // if we have a error stored, return that one
    if ( lastError ) {
      return expected<MediaSyncFacade::MediaHandle>::error( lastError );
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

  expected<MediaSyncFacade::MediaHandle> MediaSyncFacade::attachMedia( const zypp::Url &url, const ProvideMediaSpec &request )
  {
    return attachMedia( std::vector<zypp::Url>{url}, request );
  }

  expected<MediaSyncFacade::Res> MediaSyncFacade::provide(const std::vector<zypp::Url> &urls, const ProvideFileSpec &request)
  {
    using namespace zyppng::operators;

    if ( !urls.size() )
      return expected<MediaSyncFacade::Res>::error( ZYPP_EXCPT_PTR ( zypp::media::MediaException("Can not provide a file without a URL.") ));

    std::optional<expected<MediaSyncFacade::Res>> lastErr;
    for ( zypp::Url file_url : urls ) {

      zypp::Url url(file_url);
      zypp::Pathname fileName(url.getPathName());
      url.setPathName ("/");

      expected<MediaSyncFacade::Res> res = attachMedia( urls, ProvideMediaSpec( "" ) )
          | and_then( [&, this]( MediaSyncFacade::MediaHandle &&handle ) {
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
      return expected<MediaSyncFacade::Res>::error(std::current_exception());
    } catch (...) {
      return expected<MediaSyncFacade::Res>::error(std::current_exception());
    }
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

  expected<zypp::ManagedFile> MediaSyncFacade::copyFile(zyppng::MediaSyncFacade::Res &&source, const zypp::Pathname &target)
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
