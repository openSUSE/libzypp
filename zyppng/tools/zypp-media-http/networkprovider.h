/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_TOOLS_NETWORKPROVIDER_H_INCLUDED
#define ZYPP_NG_TOOLS_NETWORKPROVIDER_H_INCLUDED

#include "zypp-curl/transfersettings.h"
#include <zypp-media/ng/worker/ProvideWorker>
#include <zypp-core/ng/base/Signals>
#include <zypp-core/ng/base/AutoDisconnect>
#include <zypp-curl/ng/network/AuthData>
#include <zypp-curl/ng/network/zckhelper.h>
#include <zypp-media/auth/CredentialManager>

#include <chrono>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS(NetworkRequestDispatcher);
  ZYPP_FWD_DECL_TYPE_WITH_REFS(NetworkRequest);
}

class NetworkProvider;

struct NetworkProvideItem : public zyppng::worker::ProvideWorkerItem
{
public:

  NetworkProvideItem( NetworkProvider &parent, zyppng::ProvideMessage &&spec );
  ~NetworkProvideItem() override;

  void startDownload( zypp::Url url );
  void cancelDownload ();

  const std::optional<zyppng::NetworkRequestError> &error() const;

  zyppng::NetworkRequestRef _dl;

  zypp::Url      _url;
  zypp::Pathname _targetFileName;
  zypp::Pathname _stagingFileName;
  bool _checkExistsOnly = false;
  std::optional<zypp::ByteCount> _expFilesize;
  std::optional<zypp::ByteCount> _headerSize;
  std::optional<zypp::Pathname> _deltaFile;

  std::chrono::steady_clock::time_point _scheduleAfter = std::chrono::steady_clock::time_point::min();

private:
  void normalDownload ();
  void clearConnections ();
  void setFinished ();
  void onStarted      ( zyppng::NetworkRequest & );
  void onFinished     (zyppng::NetworkRequest & result , const zyppng::NetworkRequestError &);
  void onAuthRequired ( zyppng::NetworkRequest &,  zyppng::NetworkAuthData &auth, const std::string &availAuth );

  zyppng::NetworkRequestError safeFillSettingsFromURL(zypp::Url &url, zypp::media::TransferSettings &set);

#ifdef ENABLE_ZCHUNK_COMPRESSION
  zyppng::ZckLoaderRef _zchunkLoader;
  void onZckBlocksRequired ( const std::vector<zyppng::ZckLoader::Block> &requiredBlocks );
  void onZckFinished ( zyppng::ZckLoader::PrepareResult result );
#endif

private:
  std::vector<zyppng::connection> _connections;
  NetworkProvider &_parent;

  std::optional<zyppng::NetworkRequestError> _lastError;

  bool   _emittedStart  = false; // < flag to make sure we emit started() only once
  time_t _authTimestamp = 0; //< timestamp of the AuthData we tried already
};

using NetworkProvideItemRef = std::shared_ptr<NetworkProvideItem>;

class NetworkProvider : public zyppng::worker::ProvideWorker
{
public:
  NetworkProvider( std::string_view workerName );
  void immediateShutdown() override;

protected:
  // ProvideWorker interface
  zyppng::expected<zyppng::worker::WorkerCaps> initialize(const zyppng::worker::Configuration &conf) override;
  void provide() override;
  void cancel(const std::deque<zyppng::worker::ProvideWorkerItemRef>::iterator &i ) override;
  zyppng::worker::ProvideWorkerItemRef makeItem(zyppng::ProvideMessage &&spec) override;

  friend struct NetworkProvideItem;
  void itemStarted  ( NetworkProvideItemRef item );
  void itemFinished ( NetworkProvideItemRef item );
  void itemAuthRequired (NetworkProvideItemRef item, zyppng::NetworkAuthData &auth, const std::string &);

private:
  zyppng::NetworkRequestDispatcherRef  _dlManager;
  zypp::Pathname _attachPoint;
  zypp::media::CredentialManager::CredentialSet _credCache; //< the credential cache for this download
};





#endif
