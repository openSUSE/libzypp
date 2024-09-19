/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ZYppFactory.cc
 *
*/
extern "C"
{
#include <sys/file.h>
}
#include <iostream>
#include <signal.h>

#include <zypp/base/Logger.h>
#include <zypp/base/LogControl.h>
#include <zypp/base/IOStream.h>
#include <zypp/base/Functional.h>
#include <zypp/base/Backtrace.h>
#include <zypp/base/LogControl.h>
#include <zypp/PathInfo.h>

#include <zypp/ZYppFactory.h>
#include <zypp/zypp_detail/ZYppImpl.h>
#include <zypp/zypp_detail/ZyppLock.h>


#include <utility>

#include <iostream>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  namespace sighandler
  {
    /// Signal handler logging a stack trace
    template <int SIG>
    class SigBacktraceHandler
    {
      static void backtraceHandler( int sig ) {
        INT << "Error: signal " << SIG << endl << dumpBacktrace << endl;
        base::LogControl::instance().emergencyShutdown();
        ::signal( SIG, lastSigHandler );
      }
      static ::sighandler_t lastSigHandler;
    };
    template <int SIG>
    ::sighandler_t SigBacktraceHandler<SIG>::lastSigHandler { ::signal( SIG, SigBacktraceHandler::backtraceHandler ) };

    // Explicit instantiation installs the handler:
    template class SigBacktraceHandler<SIGSEGV>;
    template class SigBacktraceHandler<SIGABRT>;
  }

  ///////////////////////////////////////////////////////////////////
  namespace
  {
    static weak_ptr<ZYpp>		_theZYppInstance;

    boost::shared_ptr<zypp_detail::ZYppImpl> makeZyppImpl() {
      boost::shared_ptr<zypp_detail::ZYppImpl> inst( new zypp_detail::ZYppImpl());
      return inst;
    }

    boost::shared_ptr<zypp_detail::ZYppImpl> assertZyppImpl() {
      // using boost shared_ptr to not break ABI
      static boost::shared_ptr<zypp_detail::ZYppImpl> theInstance( makeZyppImpl() );
      return theInstance;
    }
  } //namespace
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZYpp
  //
  ///////////////////////////////////////////////////////////////////

  ZYpp::ZYpp( const Impl_Ptr & impl_r )
  : _pimpl( impl_r )
  {
    zypp_detail::GlobalStateHelper::context ()->clearRepoVariables();	// upon re-acquiring the lock...
    MIL << "ZYpp is on..." << endl;
  }

  ZYpp::~ZYpp()
  {
    // here unload everything from the pool belonging to context, repos and target
    zypp_detail::GlobalStateHelper::unlock ();
    MIL << "ZYpp is off..." << endl;
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZYppFactoryException
  //
  ///////////////////////////////////////////////////////////////////

  ZYppFactoryException::ZYppFactoryException( std::string msg_r, pid_t lockerPid_r, std::string lockerName_r )
    : Exception( std::move(msg_r) )
    , _lockerPid( lockerPid_r )
    , _lockerName( std::move( lockerName_r ) )
  {}

  ZYppFactoryException::~ZYppFactoryException() throw ()
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZYppFactory
  //
  ///////////////////////////////////////////////////////////////////

  ZYppFactory ZYppFactory::instance()
  { return ZYppFactory(); }

  ZYppFactory::ZYppFactory()
  {}

  ZYppFactory::~ZYppFactory()
  {}

  ///////////////////////////////////////////////////////////////////
  //
  ZYpp::Ptr ZYppFactory::getZYpp() const
  {
    ZYpp::Ptr instance = _theZYppInstance.lock();
    if ( ! instance )
    {
      // first aquire the lock, will throw if zypp is locked already
      // The Zypp destructor will release the lock again
      zypp_detail::GlobalStateHelper::lock();

      try {
        // Here we go...
        instance.reset( new ZYpp( assertZyppImpl() ) );
        _theZYppInstance = instance;
      } catch (...) {
        // should not happen but just to be safe, if no instance is returned release the lock
        if ( !instance ) zypp_detail::GlobalStateHelper::unlock();
        throw;
      }
    }
    return instance;
  }

  ///////////////////////////////////////////////////////////////////
  //
  bool ZYppFactory::haveZYpp() const
  { return !_theZYppInstance.expired(); }

  zypp::Pathname ZYppFactory::lockfileDir()
  {
    return zypp_detail::GlobalStateHelper::lockfileDir();
  }

  /******************************************************************
  **
  **	FUNCTION NAME : operator<<
  **	FUNCTION TYPE : std::ostream &
  */
  std::ostream & operator<<( std::ostream & str, const ZYppFactory & obj )
  {
    return str << "ZYppFactory";
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
