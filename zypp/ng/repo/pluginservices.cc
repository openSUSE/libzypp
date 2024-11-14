/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "pluginservices.h"

#include <iostream>
#include <sstream>
#include <zypp/base/Logger.h>
#include <zypp/base/Gettext.h>
#include <zypp/base/String.h>
#include <zypp-core/base/InputStream>
#include <zypp-core/base/UserRequestException>

#include <zypp/ng/serviceinfo.h>
#include <zypp/ng/repoinfo.h>
#include <zypp/PathInfo.h>

using std::endl;
using std::stringstream;

///////////////////////////////////////////////////////////////////
namespace zyppng
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace repo
  { /////////////////////////////////////////////////////////////////

    class PluginServices::Impl
    {
    public:
      static void loadServices(ContextBaseRef ctx, const zypp::Pathname &path,
                               const PluginServices::ProcessService &callback );
    };

    void PluginServices::Impl::loadServices( ContextBaseRef ctx,
                                  const zypp::Pathname &path,
                                  const PluginServices::ProcessService & callback/*,
                                  const ProgressData::ReceiverFnc &progress*/ )
    {
      std::list<zypp::Pathname> entries;
      if (zypp::PathInfo(path).isExist())
      {
        if ( zypp::filesystem::readdir( entries, path, false ) != 0 )
        {
          // TranslatorExplanation '%s' is a pathname
            ZYPP_THROW(zypp::Exception(zypp::str::form(_("Failed to read directory '%s'"), path.c_str())));
        }

        //str::regex allowedServiceExt("^\\.service(_[0-9]+)?$");
        for_(it, entries.begin(), entries.end() )
        {
          ServiceInfo service_info(ctx);
          service_info.setAlias((*it).basename());
          zypp::Url url;
          url.setPathName((*it).asString());
          url.setScheme("file");
          service_info.setUrl(url);
          service_info.setType(zypp::repo::ServiceType::PLUGIN);
          service_info.setAutorefresh( true );
          DBG << "Plugin Service: " << service_info << endl;
          callback(service_info);
        }

      }
    }

    PluginServices::PluginServices(ContextBaseRef ctx, const zypp::Pathname &path,
                                   const ProcessService & callback/*,
                                                                 const ProgressData::ReceiverFnc &progress */)
    {
      Impl::loadServices(ctx, path, callback/*, progress*/);
    }

    PluginServices::~PluginServices()
    {}

    std::ostream & operator<<( std::ostream & str, const PluginServices & obj )
    {
      return str;
    }

    /////////////////////////////////////////////////////////////////
  } // namespace repo
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
