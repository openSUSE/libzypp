/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file   zypp/base/WatchPath.h
 *
*/
#ifndef ZYPP_BASE_WATCHPATH_H
#define ZYPP_BASE_WATCHPATH_H

#include "zypp/base/PtrTypes.h"
#include "zypp/Pathname.h"

namespace zypp 
{

/**
 * This class monitors a file or directory
 * for changes and allows to check whether
 * a change has occurred since the last check
 *
 * <code>
 * WatchPath watch("/somedir");
 * ...
 * if (watch.hasChanged()) {
 * ...
 * }
 * </code> 
 *
 * The implementation uses inotify and does a recursive
 * scan of the given path if it is a directory.
 * 
 * Use it when knowing that the file/folder did not change
 * could save you from expensive processing, where expensive
 * means, more expensive than reading the directory content once.
 *
 * @author Duncan Mac-Vicar P. <dmacvicar@suse.de>
 */
class WatchPath
{
  public:
    /**
     * Creates a watcher on \ref path_r
     * \ref path_r can be a file or a directory
     *
     * If \ref patch_r is a directory, it is watched
     * recursively.
     */
    WatchPath( const Pathname & path_r );

    ~WatchPath();

    bool hasChanged();

  private:
    class Impl;
    shared_ptr<Impl> _pimpl;
};


}

#endif