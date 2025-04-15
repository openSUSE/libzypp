/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_MEDIA_DETAIL_MEDIANETWORKREQUEST_EXECUTOR_INCLUDED
#define ZYPP_MEDIA_DETAIL_MEDIANETWORKREQUEST_EXECUTOR_INCLUDED

#include <zypp/ZYppCallbacks.h>
#include <zypp-core/zyppng/base/zyppglobal.h>
#include <zypp-core/zyppng/base/signals.h>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS(EventDispatcher);
  ZYPP_FWD_DECL_TYPE_WITH_REFS(NetworkRequestDispatcher);
  ZYPP_FWD_DECL_TYPE_WITH_REFS(NetworkRequest);
}

namespace zypp::media {
  class TransferSettings;
}

namespace zypp::internal
{

  /*!
   * Simple helper class to execute a \ref zyppng::NetworkRequest in a MediaHandler
   * compatible way. E.g. throwing the expected exepctions on error, triggering the reports
   * and authentication requests to the user.
   * \internal
   */
  class MediaNetworkRequestExecutor
  {
  public:

    MediaNetworkRequestExecutor();
    void executeRequest(zyppng::NetworkRequestRef &req, callback::SendReport<media::DownloadProgressReport> *report = nullptr );

    zyppng::SignalProxy<void( const zypp::Url &url, media::TransferSettings &settings, const std::string &availAuthTypes, bool firstTry, bool &canContinue )> sigAuthRequired() {
      return _sigAuthRequired;
    }

  protected:
    zyppng::Signal<void( const zypp::Url &url, media::TransferSettings &settings, const std::string &availAuthTypes, bool firstTry, bool &canContinue )> _sigAuthRequired;
    zyppng::EventDispatcherRef _evDispatcher; //< keep the ev dispatcher alive as long as MediaCurl2 is
    zyppng::NetworkRequestDispatcherRef _nwDispatcher; //< keep the dispatcher alive as well
  };

}


#endif
