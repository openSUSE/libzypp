#include "MediaHandlerFactory.h"


#include <zypp/base/Logger.h>

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

  std::unique_ptr<MediaHandler> MediaHandlerFactory::createHandler(const std::vector<MediaUrl> &o_url, const Pathname &preferred_attach_point )
  {
    if ( o_url.empty() ) {
      MIL << "Url list is empty" << std::endl;
      ZYPP_THROW(MediaException("Can not create a MediaHandler without a Url."));
    }

    std::optional<MediaUrl> primary; // primary URL, this one dictates the initial schmeme and so the handler to be used
    std::vector<MediaUrl> resolved;  // all Mirrors
    std::optional<MediaHandlerFactory::MediaHandlerType> hdlType; // Handler type as detected from primary
    std::for_each( o_url.begin (), o_url.end(), [&]( const MediaUrl &u ){

      if( !u.url().isValid() ) {
        MIL << "Url is not valid" << std::endl;
        ZYPP_THROW(MediaBadUrlException(u.url()));
      }

      UrlResolverPlugin::HeaderList custom_headers;
      if ( u.hasConfig("http-headers") )
        custom_headers = u.getConfig<UrlResolverPlugin::HeaderList>( "http-headers" );

      Url url = UrlResolverPlugin::resolveUrl(u.url(), custom_headers);
      MIL << "Trying scheme '" << url.getScheme() << "'" << std::endl;

      auto myHdlType = handlerType( url );
      if ( !myHdlType ) {
        ZYPP_THROW(MediaUnsupportedUrlSchemeException(url));
      }
      if ( !hdlType )
        hdlType = myHdlType;
      else if ( myHdlType != hdlType ) {
        // we ignore if we have a Url handler different than the primary one
        // Urls should be grouped by type already from the calling code
        MIL << "Different handler type than primary URL, ignoring" << std::endl;
        return;
      }

      MediaUrl newUrl( url, u.config()) ; // keep settings that were passed in
      if ( !custom_headers.empty () ) {
        newUrl.setConfig ( "http-headers", std::move(custom_headers) );
      }

      if ( !primary )
        primary = newUrl;

      resolved.push_back (std::move(newUrl));
    });

    // should not happen, we will at least always have the primary Url here.
    // But for completeness we check
    if ( !primary ) {
      ZYPP_THROW(MediaException("No valid Url left after resolving."));
    }

    if ( resolved.size() && *hdlType != MediaCURLType )
      ERR << "Got mirrors for handler type: " << *hdlType << " they will be ignored!" << std::endl;

    std::unique_ptr<MediaHandler> _handler;
    switch(*hdlType) {
      case MediaCDType: {
        _handler = std::make_unique<MediaCD> (*primary,preferred_attach_point);
        break;
      }
      case MediaNFSType: {
        _handler = std::make_unique<MediaNFS> (*primary,preferred_attach_point);
        break;
      }
      case MediaISOType: {
        _handler = std::make_unique<MediaISO> (*primary,preferred_attach_point);
        break;
      }
      case MediaFileType: {
        _handler = std::make_unique<MediaDIR> (*primary,preferred_attach_point);
        break;
      }
      case MediaDISKType: {
        _handler = std::make_unique<MediaDISK> (*primary,preferred_attach_point);
        break;
      }
      case MediaCIFSType: {
        _handler = std::make_unique<MediaCIFS> (*primary,preferred_attach_point);
        break;
      }
      case MediaCURLType: {
        enum WhichHandler { choose, curl, curl2, multicurl };
        WhichHandler which = choose;
        // Leagcy: choose handler in UUrl query
        if ( const std::string & queryparam = primary->url().getQueryParam("mediahandler"); ! queryparam.empty() ) {
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
          auto getenvIs = []( std::string_view var, std::string_view val )->bool {
            const char * v = ::getenv( var.data() );
            return v && v == val;
          };

          which = curl;

          if ( getenvIs( "ZYPP_CURL2", "1" ) ) {
            WAR << "Curl2 manually selected." << std::endl;
            which = curl2;
          }

        }
        // Finally use the default
        std::unique_ptr<MediaNetworkCommonHandler> handler;
        switch ( which ) {
          default:
          case curl:
            handler = std::make_unique<MediaCurl>( *primary, resolved, preferred_attach_point );
            break;
          case curl2:
            handler = std::make_unique<MediaCurl2>( *primary, resolved, preferred_attach_point );
            break;
        }
        _handler = std::move(handler);
        break;
      }
      case MediaPluginType: {
        // bsc#1228208: MediaPluginType must be resolved to a valid schema by the
        // above UrlResolverPlugin::resolveUrl call. MediaPlugin exists as a stub,
        // but is not a usable handler type.
        ZYPP_THROW(MediaUnsupportedUrlSchemeException(primary->url()));
        break;
      }
    }

    if ( !_handler ) {
      ZYPP_THROW(MediaUnsupportedUrlSchemeException(primary->url()));
    }

    // check created handler
    if ( !_handler ){
      ERR << "Failed to create media handler" << std::endl;
      ZYPP_THROW(MediaSystemException(primary->url(), "Failed to create media handler"));
    }

    MIL << "Opened: " << *_handler << std::endl;
    return _handler;
  }

}
