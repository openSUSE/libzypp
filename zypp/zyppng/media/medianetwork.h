/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPPNG_MEDIA_MEDIANETWORK_H_INCLUDED
#define ZYPPNG_MEDIA_MEDIANETWORK_H_INCLUDED

#include <zypp/media/MediaHandler.h>
#include <zypp/zyppng/core/Url>
#include <zypp/zyppng/media/network/AuthData>
#include <zypp/zyppng/media/network/TransferSettings>
#include <zypp/zyppng/messages.pb.h>
#include <zypp/zyppng/io/private/iobuffer_p.h>
#include <zypp/TmpPath.h>
#include <optional>
#include <variant>

namespace zyppng {

class Download;
class Downloader;
class Socket;
class EventDispatcher;

using RequestId = uint32_t;

class MediaHandlerNetwork : public zypp::media::MediaHandler
{
public:
  MediaHandlerNetwork(const Url &url_r, const zypp::Pathname &attach_point_hint_r);
  virtual ~MediaHandlerNetwork() override;

  TransferSettings & settings();

  // MediaHandler interface
protected:
  void attachTo(bool next) override;
  void releaseFrom(const std::string &ejectDev) override;
  void getFile(const zypp::filesystem::Pathname &filename, const zypp::ByteCount &expectedFileSize_r) const override;
  void getFiles(const std::vector<std::pair<zypp::filesystem::Pathname, zypp::ByteCount> > &files) const override;
  void getDir(const zypp::filesystem::Pathname &dirname, bool recurse_r) const override;
  void getDirInfo(std::list<std::string> &retlist, const zypp::filesystem::Pathname &dirname, bool dots) const override;
  void getDirInfo(zypp::filesystem::DirContent &retlist, const zypp::filesystem::Pathname &dirname, bool dots) const override;
  bool getDoesFileExist(const zypp::filesystem::Pathname &filename) const override;
  bool checkAttachPoint(const zypp::Pathname &apoint) const override;


  Url getFileUrl(const zypp::Pathname &filename_r) const;

private:
  struct ProgressData;
  struct DispatchContext;
  struct Request;

  std::unique_ptr<DispatchContext> ensureConnected () const;
  bool retry ( DispatchContext &ctx, Request &req ) const;
  void retryRequest ( DispatchContext &ctx, Request &req ) const;
  void handleStreamMessage ( DispatchContext &ctx, const zypp::proto::Envelope &e ) const;
  void handleRequestResult ( const Request &req , const zypp::filesystem::Pathname &filename ) const;

  Request makeRequest ( const zypp::filesystem::Pathname &filename, const zypp::ByteCount &expectedFileSize_r = {}, const zypp::filesystem::Pathname &deltaFile = {} ) const;
  void trackRequest ( DispatchContext &ctx, Request &req ) const ;

  Request *findRequest ( const zyppng::Url url ) const;
  Request *findRequest ( const RequestId id ) const;


private:
  zypp::filesystem::TmpDir   _workingDir;
  mutable TransferSettings   _settings;
  mutable RequestId          _nextRequestId = 0;
  mutable std::list<Request> _requests;
  mutable std::optional<int> _socket;
  mutable IOBuffer           _readBuffer;

  // MediaHandler interface
protected:
  void disconnectFrom() override;

  // MediaHandler interface
public:
  void precacheFiles(const std::vector<zypp::OnMediaLocation> &files) override;
};

}



#endif
