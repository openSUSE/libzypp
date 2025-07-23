#include "private/threaddata_p.h"
#include "private/eventdispatcher_glib_p.h"
#include <zypp-core/base/Logger.h>
#include <ostream> //for std::endl
#include <sstream>
#include <pthread.h>

namespace zyppng
{
  ThreadData::ThreadData()
  : _threadId( std::this_thread::get_id() ),
    _nativeHandle( pthread_self() )
  {
  }

  ThreadData &ThreadData::current()
  {
    static thread_local ThreadData data;
    return data;
  }

  const std::string &ThreadData::name() const
  {
    if ( _threadName.empty() ) {
      std::stringstream strStr;
      strStr << _threadId;
      _threadName = strStr.str();
    }
    return _threadName;
  }

  std::shared_ptr<EventDispatcher> ThreadData::ensureDispatcher()
  {
    auto sp = _dispatcher.lock();
    if (!sp) {
      MIL << "Creating the Event Dispatcher for thread: " << name() << "("<<_threadId<<")" << std::endl;
      _dispatcher = sp = EventDispatcherPrivate::create();
    }
    return sp;
  }

  void ThreadData::setDispatcher( const std::shared_ptr<EventDispatcher> &disp )
  {
    if ( _dispatcher.lock() ) {
      WAR << "Dispatcher was already created for the current thread" << std::endl;
      return;
    }
    _dispatcher = disp;
  }

  void ThreadData::syncNativeName()
  {
    // length is restricted to 16 characters, including the terminating null byte ('\0')
    pthread_setname_np( _nativeHandle, name().substr(0,15).c_str() );
  }

  std::shared_ptr<EventDispatcher> ThreadData::dispatcher() {
    auto sp = _dispatcher.lock();
    if (!sp) {
      MIL << "Requested Event Dispatcher for thread: " << name() << "("<<_threadId<<") but none was created." << std::endl;
    }
    return sp;
  }
} // namespace zyppng
