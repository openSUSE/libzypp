/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPPNG_PRIVATE_DOWNLOADER_P_H
#define ZYPPNG_PRIVATE_DOWNLOADER_P_H

#include <zyppng/downloader.h>
#include <zypp-media/ng/Provide>
#include <zyppng/utils/GObjectMemory>
#include <zyppng/utils/GioMemory>

struct _ZyppDownloader
{
  GObjectClass            parent_class;
  struct Cpp {
    ZyppContext *context  = nullptr;
    zyppng::ProvideRef _provider;
    std::vector<zyppng::util::GObjectSPtr<GTask>> _runningTasks;
  } _data;
};

ZyppDownloader *zypp_downloader_new( ZyppContext *ctx );


#endif // ZYPPNG_PRIVATE_DOWNLOADER_P_H
