#include "MediaHandlerFactory.h"

#include <zypp-core/APIConfig.h>
#include <zypp-core/ng/pipelines/expected.h>
#include <zypp-core/ng/pipelines/transform.h>
#include <zypp-core/base/Logger.h>
#include <zypp-core/base/Env.h>

#include <zypp-media/MediaException>
#include <zypp/media/MediaHandler.h>

#include <zypp/media/MediaNFS.h>
#include <zypp/media/MediaCD.h>
#include <zypp/media/MediaDIR.h>
#include <zypp/media/MediaDISK.h>
#include <zypp/media/MediaCIFS.h>
#include <zypp/media/MediaCurl.h>
#include <zypp/media/MediaCurl2.h>
#include <zypp/media/MediaISO.h>
#include <zypp/media/MediaPlugin.h>
#include <zypp/media/UrlResolverPlugin.h>

#include <optional>

namespace zypp::media {

  MediaHandlerFactory::MediaHandlerFactory()
  {

  }

  std::optional<MediaHandlerFactory::MediaHandlerType> MediaHandlerFactory::handlerType(const Url &url)
  {
    std::string scheme = url.getScheme();
    if (scheme == "cd" || scheme == "dvd")
      return MediaCDType;
    else if (scheme == "nfs" || scheme == "nfs4")
      return MediaNFSType;
    else if (scheme == "iso")
      return MediaISOType;
    else if (scheme == "file" || scheme == "dir")
      return MediaFileType;
    else if (scheme == "hd" )
      return MediaDISKType;
    else if (scheme == "cifs" || scheme == "smb")
      return MediaCIFSType;
    else if (scheme == "ftp" || scheme == "tftp" || scheme == "http" || scheme == "https")
      return MediaCURLType;
    else if (scheme == "plugin" )
      return MediaPluginType;
    return {};
  }

  std::unique_ptr<MediaHandler> MediaHandlerFactory::createHandler( const MirroredOrigin &origin, const Pathname &preferred_attach_point )
  {
    using namespace zyppng::operators;

    if ( !origin.isValid() ) {
      MIL << "MirroredOrigin is invalid" << std::endl;
      ZYPP_THROW(MediaException("Can not create a MediaHandler without a authority Url."));
    }

    std::optional<MirroredOrigin> resolvedOrigin;
    std::optional<MediaHandlerFactory::MediaHandlerType> hdlType; // Handler type as detected from primary

    const auto &resolveEndpoint = [&]( const OriginEndpoint &u ) {
      if( !u.isValid() ) {
        MIL << "OriginEndpoint is not valid" << std::endl;
        return zyppng::expected<OriginEndpoint>::error( ZYPP_EXCPT_PTR(MediaBadUrlException(u.url())) );
      }

      UrlResolverPlugin::HeaderList custom_headers;
      if ( u.hasConfig("http-headers") )
        custom_headers = u.getConfig<UrlResolverPlugin::HeaderList>( "http-headers" );

      Url url = UrlResolverPlugin::resolveUrl(u.url(), custom_headers);
      MIL << "Trying scheme '" << url.getScheme() << "'" << std::endl;

      if ( !handlerType( url ) ) {
        return zyppng::expected<OriginEndpoint>::error( ZYPP_EXCPT_PTR(MediaUnsupportedUrlSchemeException(url)) );
      }

      OriginEndpoint resolvedEndpoint( url, u.config() ); // keep settings that were passed in
      if ( !custom_headers.empty () ) {
        resolvedEndpoint.setConfig ( "http-headers", std::move(custom_headers) );
      }

      return zyppng::expected<OriginEndpoint>::success(resolvedEndpoint);
    };

    using zyppng::operators::operator |;
    (resolveEndpoint( origin.authority() ) // first resolve the authority, that one needs to work always
    | and_then([&]( OriginEndpoint ep ) {
      hdlType = handlerType ( ep.url() );
      resolvedOrigin = MirroredOrigin( std::move(ep) );

      // we have a valid origin, now resolve everything else
      return zyppng::transform_collect( std::vector<OriginEndpoint>(origin.mirrors ()), resolveEndpoint );

    })
    | and_then([&]( std::vector<OriginEndpoint> resolvedMirrs ) {
      std::for_each( resolvedMirrs.begin (), resolvedMirrs.end(), [&]( const OriginEndpoint &rEp ){

        if ( handlerType (rEp.url()) != hdlType ) {
          // we ignore if we have a Url handler different than the primary one
          // Urls should be grouped by type already from the calling code
          MIL << "Different handler type than primary URL, ignoring" << std::endl;
          return;
        }
        resolvedOrigin->addMirror(rEp);

      });
      return zyppng::expected<void>::success();

    })).unwrap(); // throw on error


    // should not happen, we will at least always have the primary Url here.
    // But for completeness we check
    if ( !resolvedOrigin ) {
      ZYPP_THROW(MediaException("No valid Url left after resolving."));
    }

    if ( resolvedOrigin->endpointCount() > 1 && *hdlType != MediaCURLType )
      ERR << "Got mirrors for handler type: " << *hdlType << " they will be ignored!" << std::endl;

    std::unique_ptr<MediaHandler> _handler;
    switch(*hdlType) {
      case MediaCDType: {
        _handler = std::make_unique<MediaCD> (*resolvedOrigin,preferred_attach_point);
        break;
      }
      case MediaNFSType: {
        _handler = std::make_unique<MediaNFS> (*resolvedOrigin,preferred_attach_point);
        break;
      }
      case MediaISOType: {
        _handler = std::make_unique<MediaISO> (*resolvedOrigin,preferred_attach_point);
        break;
      }
      case MediaFileType: {
        _handler = std::make_unique<MediaDIR> (*resolvedOrigin,preferred_attach_point);
        break;
      }
      case MediaDISKType: {
        _handler = std::make_unique<MediaDISK> (*resolvedOrigin,preferred_attach_point);
        break;
      }
      case MediaCIFSType: {
        _handler = std::make_unique<MediaCIFS> (*resolvedOrigin,preferred_attach_point);
        break;
      }
      case MediaCURLType: {
        enum WhichHandler { choose, curl, curl2 };
        WhichHandler which = choose;
        // Leagcy: choose handler in Url query
        if ( const std::string & queryparam = resolvedOrigin->authority().url().getQueryParam("mediahandler"); ! queryparam.empty() ) {
          if ( queryparam == "curl" )
            which = curl;
          else if ( queryparam == "curl2" )
            which = curl2;
          else if ( queryparam == "network" ||  queryparam == "multicurl" )
            which = choose; // old backends, choose default
          else
            WAR << "Unknown mediahandler='" << queryparam << "' in URL; Choosing the default" << std::endl;
        }
        // Otherwise choose handler through ENV
        if ( which == choose ) {
          TriBool envstate = env::getenvBool( "ZYPP_CURL2" );
          if ( indeterminate(envstate) ) {
#if APIConfig(LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT)
            which = curl;
#else
            which = curl2;
#endif
          } else {
            which = bool(envstate) ? curl2 : curl;
          }
        }
        // Finally use the default
        std::unique_ptr<MediaNetworkCommonHandler> handler;
        switch ( which ) {
          default:
          case curl2:
            handler = std::make_unique<MediaCurl2>( *resolvedOrigin, preferred_attach_point );
            break;
          case curl:
            handler = std::make_unique<MediaCurl>( *resolvedOrigin, preferred_attach_point );
            break;
        }
        _handler = std::move(handler);
        break;
      }
      case MediaPluginType: {
        // bsc#1228208: MediaPluginType must be resolved to a valid schema by the
        // above UrlResolverPlugin::resolveUrl call. MediaPlugin exists as a stub,
        // but is not a usable handler type.
        ZYPP_THROW(MediaUnsupportedUrlSchemeException(resolvedOrigin->authority().url()));
        break;
      }
    }

    if ( !_handler ) {
      ZYPP_THROW(MediaUnsupportedUrlSchemeException(resolvedOrigin->authority().url()));
    }

    // check created handler
    if ( !_handler ){
      ERR << "Failed to create media handler" << std::endl;
      ZYPP_THROW(MediaSystemException(resolvedOrigin->authority().url(), "Failed to create media handler"));
    }

    MIL << "Opened: " << *_handler << std::endl;
    return _handler;
  }

}
