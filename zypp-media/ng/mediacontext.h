/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_MEDIA_NG_MEDIACONTEXT_H_INCLUDED
#define ZYPP_MEDIA_NG_MEDIACONTEXT_H_INCLUDED

#include <zypp-core/zyppng/ui/userinterface.h>

namespace zypp {
  class MediaConfig;
}

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( MediaContext );

  class MediaContext : public UserInterface {
  public:
    virtual zypp::Pathname contextRoot() const  = 0;
    virtual zypp::MediaConfig &mediaConfig() = 0;

    static zypp::Pathname defaultConfigPath(){
      return "/etc/zypp/zypp.conf";
    }
  };

}


#endif
