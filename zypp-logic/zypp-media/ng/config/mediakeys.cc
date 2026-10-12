/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include <zypp-media/ng/config/mediakeys.h>

namespace zyppng::config {

  void registerMediaKeys( Config & cfg )
  {
    cfg.registerKey( CredentialsDir );
    cfg.registerKey( CredentialsFile );
    cfg.registerKey( GeoipEnabled );
    cfg.registerKey( GeoipHostnames );
    cfg.registerKey( MaxConcurrentConnections );
    cfg.registerKey( MinDownloadSpeed );
    cfg.registerKey( MaxDownloadSpeed );
    cfg.registerKey( MaxSilentTries );
    cfg.registerKey( TransferTimeout );
    cfg.registerKey( ConnectTimeout );
    cfg.registerKey( MediaMountDir );
  }

} // namespace zyppng::config
