#include <ctime>
#include <iostream>
#include "Tools.h"

#include <zypp/base/PtrTypes.h>
#include <zypp/base/Exception.h>
#include <zypp/base/ProvideNumericId.h>

#include "zypp/ZYppFactory.h"
#include "zypp/ResPoolProxy.h"
#include <zypp/SourceManager.h>
#include <zypp/SourceFactory.h>

#include "zypp/ZYppCallbacks.h"
#include "zypp/NVRAD.h"
#include "zypp/ResPool.h"
#include "zypp/ResFilters.h"
#include "zypp/CapFilters.h"
#include "zypp/Package.h"
#include "zypp/Pattern.h"
#include "zypp/Language.h"
#include "zypp/NameKindProxy.h"
#include "zypp/pool/GetResolvablesToInsDel.h"

#include "zypp/ZConfig.h"

using namespace std;
using namespace zypp;
using namespace zypp::ui;
using namespace zypp::functor;

///////////////////////////////////////////////////////////////////

static const Pathname sysRoot( "/Local/TEST" );

///////////////////////////////////////////////////////////////////

struct Xprint
{
  bool operator()( const PoolItem & obj_r )
  {
    SEC << obj_r << endl;
    return true;
  }
};

///////////////////////////////////////////////////////////////////

struct ConvertDbReceive : public callback::ReceiveReport<target::ScriptResolvableReport>
{
  virtual void start( const Resolvable::constPtr & script_r,
                      const Pathname & path_r,
                      Task task_r )
  {
    SEC << __FUNCTION__ << endl
    << "  " << script_r << endl
    << "  " << path_r   << endl
    << "  " << task_r   << endl;
  }

  virtual bool progress( Notify notify_r, const std::string & text_r )
  {
    SEC << __FUNCTION__ << endl
    << "  " << notify_r << endl
    << "  " << text_r   << endl;
    return true;
  }

  virtual void problem( const std::string & description_r )
  {
    SEC << __FUNCTION__ << endl
    << "  " << description_r << endl;
  }

  virtual void finish()
  {
    SEC << __FUNCTION__ << endl;
  }

};

///////////////////////////////////////////////////////////////////

struct MediaChangeReceive : public callback::ReceiveReport<media::MediaChangeReport>
{
  virtual Action requestMedia( Source_Ref source
                               , unsigned mediumNr
                               , Error error
                               , const std::string & description )
  {
    SEC << __FUNCTION__ << endl
    << "  " << source << endl
    << "  " << mediumNr << endl
    << "  " << error << endl
    << "  " << description << endl;
    return IGNORE;
  }
};

///////////////////////////////////////////////////////////////////

namespace container
{
  template<class _Tp>
    bool isIn( const std::set<_Tp> & cont, const typename std::set<_Tp>::value_type & val )
    { return cont.find( val ) != cont.end(); }
}

///////////////////////////////////////////////////////////////////

struct PoolItemSelect
{
  void operator()( const PoolItem & pi ) const
  {
    if ( pi->source().numericId() == 2 )
      pi.status().setTransact( true, ResStatus::USER );
  }
};

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

struct AddResolvables
{
  bool operator()( const Source_Ref & src ) const
  {
    getZYpp()->addResolvables( src.resolvables() );
    return true;
  }
};

///////////////////////////////////////////////////////////////////

struct SetTransactValue
{
  SetTransactValue( ResStatus::TransactValue newVal_r, ResStatus::TransactByValue causer_r )
  : _newVal( newVal_r )
  , _causer( causer_r )
  {}

  ResStatus::TransactValue   _newVal;
  ResStatus::TransactByValue _causer;

  bool operator()( const PoolItem & pi ) const
  { return pi.status().setTransactValue( _newVal, _causer ); }
};

struct StatusReset : public SetTransactValue
{
  StatusReset()
  : SetTransactValue( ResStatus::KEEP_STATE, ResStatus::USER )
  {}
};


inline bool selectForTransact( const NameKindProxy & nkp, Arch arch = Arch() )
{
  if ( nkp.availableEmpty() ) {
    ERR << "No Item to select: " << nkp << endl;
    return false;
    ZYPP_THROW( Exception("No Item to select") );
  }

  if ( arch != Arch() )
    {
      typeof( nkp.availableBegin() ) it =  nkp.availableBegin();
      for ( ; it != nkp.availableEnd(); ++it )
      {
        if ( (*it)->arch() == arch )
	  return (*it).status().setTransact( true, ResStatus::USER );
      }
    }

  return nkp.availableBegin()->status().setTransact( true, ResStatus::USER );
}

void seltest( const NameKindProxy & nks )
{
  SEC << nks << endl;
  PoolItem av( *nks.availableBegin() );
  SEC << av << endl;
  Pattern::constPtr pat( asKind<Pattern>(av.resolvable()) );
  SEC << pat << endl;
  WAR << pat->install_packages() << endl;
  MIL << pat->deps() << endl;
  MIL << pat->includes() << endl;
  MIL << pat->extends() << endl;
}

void showProd( const PoolItem & prod )
{
  Product::constPtr p( asKind<Product>(prod) );
  DBG << prod << endl;
  MIL << p << endl;
  MIL << p->distributionName() << endl;
  MIL << p->distributionEdition() << endl;
  MIL << p->installtime() << endl;
}

///////////////////////////////////////////////////////////////////
/******************************************************************
**
**      FUNCTION NAME : main
**      FUNCTION TYPE : int
*/
int main( int argc, char * argv[] )
{
  //zypp::base::LogControl::instance().logfile( "log.restrict" );
  INT << "===[START]==========================================" << endl;
  ConvertDbReceive cr;
  cr.connect();
  MediaChangeReceive mr;
  mr.connect();

  if ( 1 )
  {
    zypp::base::LogControl::TmpLineWriter shutUp;
    Source_Ref src( createSource( "dir:/mounts/mirror/SuSE/ftp.suse.com/pub/suse/i386/update/10.2" ) );
    getZYpp()->addResolvables( src.resolvables() );
  }

  if ( 1 )
  {
    zypp::base::LogControl::TmpLineWriter shutUp;
    getZYpp()->initTarget( sysRoot );
  }

  ResPool pool( getZYpp()->pool() );
  MIL << pool << endl;

  std::for_each( pool.byKindBegin<Script>(),
                 pool.byKindEnd<Script>(),
                 Print() );

  //PoolItem pi( getPi<Patch>( "fetchmsttfonts.sh" ) );
  PoolItem pi( getPi<Script>( "fetchmsttfonts.sh-2333-patch-fetchmsttfonts.sh-2" ) );
  if ( 0 && pi && pi.status().isUninstalled() )
  {
    if ( pi.status().setTransactValue( ResStatus::TRANSACT, ResStatus::USER ) )
    {
      USR << pi << endl;
      USR << "Solve: " << getZYpp()->resolver()->resolvePool() << endl;
      vdumpPoolStats( USR << "Transacting:"<< endl,
                      make_filter_begin<resfilter::ByTransact>(pool),
                      make_filter_end<resfilter::ByTransact>(pool) ) << endl;


      USR << getZYpp()->commit( ZYppCommitPolicy().syncPoolAfterCommit( true ) ) << endl;
    }
  }
  USR << "Solve: " << getZYpp()->resolver()->establishPool() << endl;
  //pi = getPi<Patch>( "fetchmsttfonts.sh" );
  pi = getPi<Script>( "fetchmsttfonts.sh-2333-patch-fetchmsttfonts.sh-2" );
  vdumpPoolStats( INT << "Installed:"<< endl,
                  make_filter_begin<resfilter::ByInstalled>(pool),
                  make_filter_end<resfilter::ByInstalled>(pool) ) << endl;
  INT << pi << endl;

  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::instance().logNothing();
  return 0;
}

