#ifndef MEDIAHANDLERFACTORY_H
#define MEDIAHANDLERFACTORY_H

#include <zypp/Pathname.h>
#include <zypp/Url.h>
#include <memory>
#include <optional>


namespace zypp::media {

  class MediaHandler;

  class MediaHandlerFactory
  {
  public:

    enum MediaHandlerType {
      MediaCDType,
      MediaNFSType,
      MediaISOType,
      MediaFileType,
      MediaDISKType,
      MediaCIFSType,
      MediaCURLType,
      MediaPluginType
    };

    MediaHandlerFactory();
    static std::unique_ptr<MediaHandler> createHandler (const Url& o_url, const Pathname & preferred_attach_point);
    static std::optional<MediaHandlerType> handlerType( const Url &url );
  };

}


#endif // MEDIAHANDLERFACTORY_H
