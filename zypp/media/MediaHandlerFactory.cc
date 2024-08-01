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
#include <zypp/media/MediaMultiCurl.h>
#include <zypp/media/MediaNetwork.h>
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

  std::unique_ptr<MediaHandler> MediaHandlerFactory::createHandler( const Url &o_url, const Pathname &preferred_attach_point )
  {
    if(!o_url.isValid()) {
      MIL << "Url is not valid" << std::endl;
      ZYPP_THROW(MediaBadUrlException(o_url));
    }

    UrlResolverPlugin::HeaderList custom_headers;
    Url url = UrlResolverPlugin::resolveUrl(o_url, custom_headers);
    MIL << "Trying scheme '" << url.getScheme() << "'" << std::endl;

    const auto hdlType = handlerType( url );
    if ( !hdlType ) {
      ZYPP_THROW(MediaUnsupportedUrlSchemeException(url));
    }

    std::unique_ptr<MediaHandler> _handler;
    switch(*hdlType) {
      case MediaCDType: {
        _handler = std::make_unique<MediaCD> (url,preferred_attach_point);
        break;
      }
      case MediaNFSType: {
        _handler = std::make_unique<MediaNFS> (url,preferred_attach_point);
        break;
      }
      case MediaISOType: {
        _handler = std::make_unique<MediaISO> (url,preferred_attach_point);
        break;
      }
      case MediaFileType: {
        _handler = std::make_unique<MediaDIR> (url,preferred_attach_point);
        break;
      }
      case MediaDISKType: {
        _handler = std::make_unique<MediaDISK> (url,preferred_attach_point);
        break;
      }
      case MediaCIFSType: {
        _handler = std::make_unique<MediaCIFS> (url,preferred_attach_point);
        break;
      }
      case MediaCURLType: {
        enum WhichHandler { choose, curl, multicurl, network };
        WhichHandler which = choose;
        // Leagcy: choose handler in UUrl query
        if ( const std::string & queryparam = url.getQueryParam("mediahandler"); ! queryparam.empty() ) {
          if ( queryparam == "network" )
            which = network;
          else if ( queryparam == "multicurl" )
            which = multicurl;
          else if ( queryparam == "curl" )
            which = curl;
          else
            WAR << "Unknown mediahandler='" << queryparam << "' in URL; Choosing the default" << std::endl;
        }
        // Otherwise choose handler through ENV
        if ( which == choose ) {
          auto getenvIs = []( std::string_view var, std::string_view val )->bool {
            const char * v = ::getenv( var.data() );
            return v && v == val;
          };

          if ( getenvIs( "ZYPP_MEDIANETWORK", "1" ) ) {
            WAR << "MediaNetwork backend enabled" << std::endl;
            which = network;
          }
          else if ( getenvIs( "ZYPP_MULTICURL", "0" ) ) {
            WAR << "multicurl manually disabled." << std::endl;
            which = curl;
          }
          else
            which = multicurl;
        }
        // Finally use the default
        std::unique_ptr<MediaNetworkCommonHandler> handler;
        switch ( which ) {
          default:
          case multicurl:
            handler = std::make_unique<MediaMultiCurl>( url, preferred_attach_point );
            break;

          case network:
            handler = std::make_unique<MediaNetwork>( url, preferred_attach_point );
            break;

          case curl:
            handler = std::make_unique<MediaCurl>( url, preferred_attach_point );
            break;
        }
        // Set up the handler
        for ( const auto & el : custom_headers ) {
          std::string header { el.first };
          header += ": ";
          header += el.second;
          MIL << "Added custom header -> " << header << std::endl;
          handler->settings().addHeader( std::move(header) );
        }
        _handler = std::move(handler);
        break;
      }
      case MediaPluginType: {
        // bsc#1228208: MediaPluginType must be resolved to a valid schema by the
        // above UrlResolverPlugin::resolveUrl call. MediaPlugin exists as a stub,
        // but is not a usable handler type.
        ZYPP_THROW(MediaUnsupportedUrlSchemeException(url));
        break;
      }
    }

    if ( !_handler ) {
      ZYPP_THROW(MediaUnsupportedUrlSchemeException(url));
    }

    // check created handler
    if ( !_handler ){
      ERR << "Failed to create media handler" << std::endl;
      ZYPP_THROW(MediaSystemException(url, "Failed to create media handler"));
    }

    MIL << "Opened: " << *_handler << std::endl;
    return _handler;
  }

}
