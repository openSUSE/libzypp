#ifndef MEDIAHANDLERFACTORY_H
#define MEDIAHANDLERFACTORY_H

#include <zypp-core/Pathname.h>
#include <zypp-core/Url.h>
#include <zypp-core/MirroredOrigin.h>
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
    static std::unique_ptr<MediaHandler> createHandler (const MirroredOrigin &origin, const Pathname & preferred_attach_point);
    static std::optional<MediaHandlerType> handlerType( const Url &url );
  };

}


#endif // MEDIAHANDLERFACTORY_H
