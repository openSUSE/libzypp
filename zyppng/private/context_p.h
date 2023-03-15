/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef CONTEXT_P_H
#define CONTEXT_P_H

#include <zyppng/context.h>
#include <zypp-core/zyppng/base/EventDispatcher>
#include <zypp-core/Pathname.h>
#include <zyppng/utils/GObjectMemory>
#include <string>

#include <zyppng/repomanager.h>
#include <zyppng/downloader.h>

// the good ol zypp API
#include <zypp/ZYppFactory.h>
#include <zypp/ZYppCallbacks.h>

/*
 * This is the GObject derived type, it is simply used to host our c++ data object and is
 * passed as a opaque pointer to the user code.
 * This is always created with malloc, so no con or destructors are executed here!
 */
struct _ZyppContext
{
  GObjectClass            parent_class;
  // adding our C++ pocket, we will call the C++ con and destructors explicitely
  struct Cpp{
    std::string version = "1.0";
    zypp::Pathname sysRoot = "/";
    zyppng::EventDispatcherRef _dispatcher;
    zyppng::ZyppRepoManagerRef _manager;
    zyppng::ZyppDownloaderRef  _downloader;
    zypp::ZYpp::Ptr godPtr;
  } _data;
};

#endif // CONTEXT_P_H
