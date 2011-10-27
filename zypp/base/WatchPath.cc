/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file   zypp/base/WatchPath.cc
 *
*/
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <map>

#include "zypp/base/Logger.h"
#include "zypp/base/String.h"
#include "zypp/base/WatchPath.h"
#include "zypp/PathInfo.h"

using namespace std;
using namespace zypp::filesystem;

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

static inline std::ostream & operator<<( std::ostream & str, struct inotify_event &e)
{
    return str << "[" << ((e.len > 0) ? e.name : "(empty)") << "|" << (int) e.cookie
      << ((e.mask & IN_ACCESS) ? ",ACCESS" : "")
      << ((e.mask & IN_ATTRIB) ? ",ATTRIB" : "")
      << ((e.mask & IN_CLOSE_WRITE)?  ",CLOSE_WRITE" : "")
      << ((e.mask & IN_CLOSE_NOWRITE)?  ",CLOSE_NOWRITE" : "")
      << ((e.mask & IN_CREATE) ? ",CREATE" : "")
      << ((e.mask & IN_DELETE) ? ",DELETE" : "")
      << ((e.mask & IN_DELETE_SELF)?  ",IN_DELETE_SELF" : "")
      << ((e.mask & IN_MODIFY) ? ",MODIFY" : "")
      << ((e.mask & IN_MOVE)?  ",MOVE" : "")
      << ((e.mask & IN_MOVE_SELF)?  ",MOVE_SELF" : "")
      << ((e.mask & IN_MOVED_FROM)?  ",MOVED_FROM" : "")
      << ((e.mask & IN_MOVED_TO)?  ",MOVED_TO" : "")
      << ((e.mask & IN_OPEN)?  ",OPEN" : "")
      << ((e.mask & IN_IGNORED)?  ",IGNORED" : "")
      
      << "(" << e.mask << ")"
      << "]";
}

// read a directory path_r recursively, storing the result
// in result
void readdir_r(const Pathname &path_r, DirContent &result)
{
  DirContent entries;
  readdir(entries, path_r, false, PathInfo::LSTAT);
  for (auto entry : entries) {
    entry.name = (path_r + entry.name).asString();
    result.push_back(entry);
    if (entry.type == FT_DIR) {
      readdir_r(entry.name, result);
    }
  }
}

namespace zypp
{

struct WatchPath::Impl {
  
  static const int MASK = (IN_DELETE_SELF | IN_CREATE | IN_MOVE | IN_CLOSE_WRITE);

  Impl(const Pathname &path_r) 
    : _fd(-1)
    , _path(path_r)
  {
    _fd = ::inotify_init();
    inotifyAddRecursive(_path);
    XXX << "watcher init at " << _path << " done" << endl;
  }

  ~Impl()
  {
    close(_fd);
  }

  void inotifyAddRecursive(const Pathname &path_r)
  {
    XXX << "add watch for: " << path_r << endl;
    int wd = inotify_add_watch(_fd, path_r.c_str(), MASK);
    _wds[wd] = path_r.asString();
    DirContent content;

    if (PathInfo(path_r).isDir()) {
      readdir_r(path_r, content);

      for (auto entry : content) {        
        int wd = inotify_add_watch(_fd, entry.name.c_str(), MASK);
        _wds[wd] = entry.name;
        XXX << "add watch for: " << entry.name << endl;
      }
    }
  }

  bool hasChanged()
  {
    bool changed = false;
    char buf[BUF_LEN];
    int len, i = 0;

    struct pollfd pfd = { _fd, POLLIN, 0 };

    // timeout of 0
    int ret = poll(&pfd, 1, 0);
    if (ret < 0) {
        ERR << "poll failed" << strerror(errno) << endl;
        return changed;
    }
    else if (ret == 0) {
        // timeout
        return changed;
    }
    
    len = read(_fd, buf, BUF_LEN);
    XXX << "read: " << len << "  bytes" << endl;
    
    if (len < 0) {
        ERR << strerror(errno) << endl;
    } else if (!len) {
        ERR << "BUF_LEN too small?" << endl;        
    }

    while (i < len) {
        struct inotify_event *event;
        event = (struct inotify_event *) &buf[i];
        XXX << *event << endl;
        
        // add new files and directories
        // to the watched list
        if (event->mask & IN_CREATE) {
            // watched directory where the file was
            // created
            string dir = _wds[event->wd];
            string name(event->name);

            if (!name.empty())
                inotifyAddRecursive(Pathname(dir) + name);
        } // remove files that are gone
        else if (event->mask & IN_DELETE_SELF) {
            string name = _wds[event->wd];
            XXX << "rm watch for delete: " << name << endl;
            inotify_rm_watch(_fd, event->wd);
            _wds.erase(event->wd);
        }
        
        i += EVENT_SIZE + event->len;

        // even if we set the mask we are interested in
        // inotify can send events like IN_IGNORED or
        // IN_Q_OVERFLOW
        if (event->mask & IN_IGNORED)
            continue;

        // for the rest, including unmount, we consider
        // it as changed
        changed = true;
    }
    return changed;
  }

  int _fd;
  Pathname _path;

  // map wd -> path
  map<int, string> _wds;
};


WatchPath::WatchPath(const Pathname &path_r)
  : _pimpl(new Impl(path_r))
{
}

WatchPath::~WatchPath()
{}    


bool WatchPath::hasChanged()
{
    return _pimpl->hasChanged();
}

}