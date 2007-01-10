#include <ctime>
#include <iostream>
#include "Tools.h"

#include <zypp/base/PtrTypes.h>
#include <zypp/base/Exception.h>
#include <zypp/base/ProvideNumericId.h>
#include <zypp/base/DefaultIntegral.h>

using namespace zypp;
#include "FakePool.h"

#include "zypp/ZYppFactory.h"
#include "zypp/ResPoolProxy.h"
#include <zypp/SourceManager.h>
#include <zypp/SourceFactory.h>
#include <zypp/VendorAttr.h>

#include "zypp/NVRAD.h"
#include "zypp/ResTraits.h"
#include "zypp/ResPool.h"
#include "zypp/ResFilters.h"
#include "zypp/CapFilters.h"
#include "zypp/CapFactory.h"
#include "zypp/Package.h"
#include "zypp/Language.h"
#include "zypp/NameKindProxy.h"
#include "zypp/pool/GetResolvablesToInsDel.h"
#include "zypp/target/rpm/RpmDb.h"


using namespace std;
using namespace zypp;
using namespace zypp::ui;
using namespace zypp::functor;
using namespace zypp::debug;
using namespace zypp::target::rpm;

///////////////////////////////////////////////////////////////////

static const Pathname sysRoot( "/Local/ROOT" );

///////////////////////////////////////////////////////////////////

struct AddResolvables
{
  bool operator()( const Source_Ref & src ) const
  {
    getZYpp()->addResolvables( src.resolvables() );
    return true;
  }
};

struct AddSource
{
  bool operator()( const std::string & url )
  {
    SourceManager::sourceManager()->addSource( createSource( url ) );
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
  {
    bool ret = pi.status().setTransactValue( _newVal, _causer );
    if ( ! ret )
      ERR << _newVal <<  _causer << " " << pi << endl;
    return ret;
  }
};

struct StatusReset : public SetTransactValue
{
  StatusReset()
  : SetTransactValue( ResStatus::KEEP_STATE, ResStatus::USER )
  {}
};

struct StatusInstall : public SetTransactValue
{
  StatusInstall()
  : SetTransactValue( ResStatus::TRANSACT, ResStatus::USER )
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

///////////////////////////////////////////////////////////////////

#include "zypp/CapMatchHelper.h"

struct GetObsoletes
{
  void operator()( const PoolItem & pi )
  {
    INT << pi << endl;
    for_each( pi->dep(Dep::OBSOLETES).begin(),
              pi->dep(Dep::OBSOLETES).end(),
              ForEachMatchInPool( getZYpp()->pool(), Dep::PROVIDES,
                                  Print() ) );
  }
};

///////////////////////////////////////////////////////////////////

template<class _Iterator>
  void addPool( _Iterator begin_r, _Iterator end_r )
  {
    DataCollect dataCollect;
    dataCollect.collect( begin_r, end_r );
    getZYpp()->addResolvables( dataCollect.installed(), true );
    getZYpp()->addResolvables( dataCollect.available() );
    vdumpPoolStats( USR << "Pool:" << endl,
                    getZYpp()->pool().begin(),
                    getZYpp()->pool().end() ) << endl;
  }

template<class _Res>
  void poolRequire( const std::string & capstr_r,
                    ResStatus::TransactByValue causer_r = ResStatus::USER )
  {
    getZYpp()->pool().additionalRequire()[causer_r]
                     .insert( CapFactory().parse( ResTraits<_Res>::kind,
                                                  capstr_r ) );
  }

bool solve( bool establish = false )
{
  if ( establish )
    {
      bool eres = getZYpp()->resolver()->establishPool();
      if ( ! eres )
        {
          ERR << "establish " << eres << endl;
          return false;
        }
      MIL << "establish " << eres << endl;
    }

  bool rres = getZYpp()->resolver()->resolvePool();
  if ( ! rres )
    {
      ERR << "resolve " << rres << endl;
      return false;
    }
  MIL << "resolve " << rres << endl;
  return true;
}

      struct StorageRemoveObsoleted
      {
        StorageRemoveObsoleted( const PoolItem & byPoolitem_r )
        : _byPoolitem( byPoolitem_r )
        {}

        bool operator()( const PoolItem & poolitem_r ) const
        {
          if ( ! poolitem_r.status().isInstalled() )
            return true;

          if ( isKind<Package>(poolitem_r.resolvable()) )
            {
              ERR << "Ignore unsupported Package/non-Package obsolete: "
                  << _byPoolitem << " obsoletes " << poolitem_r << endl;
              return true;
            }

          INT << poolitem_r << " by " << _byPoolitem << endl;

          return true;
        }

      private:
        const PoolItem               _byPoolitem;
      };

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

struct IMediaKey
{
  IMediaKey()
  {}

  IMediaKey( const Source_Ref & source_r, unsigned mediaNr_r )
  : _source( source_r )
  , _mediaNr( mediaNr_r )
  {}

  bool operator==( const IMediaKey & rhs ) const
  { return( _source == rhs._source && _mediaNr == rhs._mediaNr ); }

  bool operator!=( const IMediaKey & rhs ) const
  { return ! operator==( rhs ); }

  bool operator<( const IMediaKey & rhs ) const
  {
    return( _source.numericId() < rhs._source.numericId()
            || ( _source.numericId() == rhs._source.numericId()
                 && _mediaNr < rhs._mediaNr ) );
  }

  Source_Ref                  _source;
  DefaultIntegral<unsigned,0> _mediaNr;
};

std::ostream & operator<<( std::ostream & str, const IMediaKey & obj )
{
  return str << "[" << obj._source.numericId() << "|" << obj._mediaNr << "]"
             << " " << obj._source.alias();
}

///////////////////////////////////////////////////////////////////

struct IMediaValue
{
  void add( const PoolItem & pi_r )
  {
    ++_count;
    _size += pi_r->archivesize();
  }

  DefaultIntegral<unsigned,0> _count;
  ByteCount                   _size;
};

std::ostream & operator<<( std::ostream & str, const IMediaValue & obj )
{
  return str << "[" <<  str::numstring(obj._count,3) << "|" << obj._size.asString(6) << "]";
}

///////////////////////////////////////////////////////////////////

struct IMedia
{
  typedef std::pair<IMediaKey,IMediaValue>            IMediaElem;
  typedef std::list<IMediaElem>                       IMediaSequence;

  typedef std::pair<IMediaElem,IMediaValue>           ICacheElem;
  typedef std::list<IMediaElem>                       ICacheSequence;

  void add( const PoolItem & pi )
  {
    if ( ! onInteractiveMedia( pi ) )
      return;

    IMediaKey current( pi->source(), pi->sourceMediaNr() );

    if ( current != _lastInteractive )
      {

        if ( !_iMediaSequence.empty() )
          WAR << "After " << _iMediaSequence.back().second._count << endl;

        INT << "Change from " << _lastInteractive << endl;
        INT << "         to " << current << endl;

        _lastInteractive = current;
        _iMediaSequence.push_back( std::pair<IMediaKey,IMediaValue>
                                   ( _lastInteractive, IMediaValue() ) );
      }

    _iMediaSequence.back().second.add( pi );
    DBG << "After " << _iMediaSequence.back().second._count << " - " << pi << endl;
  }

  void done()
  {
    if ( !_iMediaSequence.empty() )
      WAR << "Final " << _iMediaSequence.back().second._count << endl;
  }

  bool onInteractiveMedia( const PoolItem & pi ) const
  {
    if ( pi->sourceMediaNr() == 0 ) // no media access at all
      return false;
    std::string scheme( pi->source().url().getScheme() );
    return true || ( scheme == "dvd" || scheme == "cd" );
  }

  void ComputeCache()
  {

    for ( IMediaSequence::const_iterator it = _iMediaSequence.begin();
          it != _iMediaSequence.end(); ++it )
      {

      }
  }

  IMediaKey      _lastInteractive;
  IMediaSequence _iMediaSequence;
};

std::ostream & operator<<( std::ostream & str, const IMedia & obj )
{
  str << "Sequence:" << endl;
  for ( IMedia::IMediaSequence::const_iterator it = obj._iMediaSequence.begin();
        it != obj._iMediaSequence.end(); ++it )
    {
      str << "   " << it->first << "\t" << it->second << endl;
    }



  return str;
}


///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
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
  ResPool pool( getZYpp()->pool() );

  if ( 1 )
    {
      zypp::base::LogControl::TmpLineWriter shutUp;
      getZYpp()->initTarget( sysRoot );
    }
  MIL << "Added target: " << pool << endl;

  if ( 1 )
    {
      zypp::base::LogControl::TmpLineWriter shutUp;
      Source_Ref src;
      src = createSource( "dir:/Local/SUSE-Linux-10.1-Build_830-i386/CD1",
                          "Build_830" );
      getZYpp()->addResolvables( src.resolvables() );
      src = createSource( "dir:/Local/SUSE-Linux-10.1-Build_830-Addon-BiArch/CD1",
                          "Addon-BiArch" );
      getZYpp()->addResolvables( src.resolvables() );
    }
  MIL << "Added sources: " << pool << endl;

  // select...
  for_each( pool.byKindBegin<Product>(), pool.byKindEnd<Product>(), StatusInstall() );
#define selt(K,N) selectForTransact( nameKindProxy<K>( pool, #N ) )
  selt( Selection, default );
  selt( Package, RealPlayer );
  selt( Package, acroread );
  selt( Package, flash-player );
#undef selt


  solve();
  //vdumpPoolStats( USR << "Transacting:"<< endl,
  //                make_filter_begin<resfilter::ByTransact>(pool),
  //                make_filter_end<resfilter::ByTransact>(pool) ) << endl;

  pool::GetResolvablesToInsDel collect( pool, pool::GetResolvablesToInsDel::ORDER_BY_MEDIANR );
  typedef pool::GetResolvablesToInsDel::PoolItemList PoolItemList;
  MIL << "GetResolvablesToInsDel:" << endl << collect << endl;

  {
    const PoolItemList & items_r( collect._toInstall );


    unsigned snr = 0;

    for ( PoolItemList::const_iterator it = items_r.begin(); it != items_r.end(); it++ )
      {
        if (isKind<Package>(it->resolvable()))
          {
            Package::constPtr p = asKind<Package>(it->resolvable());
            if (it->status().isToBeInstalled())
              {
                DBG << "[" << snr << "] " << p << endl;
                ++snr;
              }
          }


      }





    IMedia m;
    for_each( collect._toInstall.begin(), collect._toInstall.end(),
              bind( &IMedia::add, ref(m), _1 ) );
    m.done();
    MIL << m << endl;
    INT << "===" << endl;
  }
  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::instance().logNothing();
  return 0;

  if ( 1 )
    {
      // Collect until the 1st package from an unwanted media occurs.
      // Further collection could violate install order.
      bool hitUnwantedMedia = false;
      PoolItemList::iterator fst=collect._toInstall.end();
      for ( PoolItemList::iterator it = collect._toInstall.begin(); it != collect._toInstall.end(); ++it)
        {
          ResObject::constPtr res( it->resolvable() );

          if ( hitUnwantedMedia
               || ( res->sourceMediaNr() && res->sourceMediaNr() != 1 ) )
            {
              if ( !hitUnwantedMedia )
                fst=it;
              hitUnwantedMedia = true;
            }
          else
            {
            }
        }
      dumpRange( WAR << "toInstall1: " << endl,
                 collect._toInstall.begin(), fst ) << endl;
      dumpRange( WAR << "toInstall2: " << endl,
                 fst, collect._toInstall.end() ) << endl;
      dumpRange( ERR << "toDelete: " << endl,
                 collect._toDelete.begin(), collect._toDelete.end() ) << endl;
    }
  else
    {
      dumpRange( WAR << "toInstall: " << endl,
                 collect._toInstall.begin(), collect._toInstall.end() ) << endl;
      dumpRange( ERR << "toDelete: " << endl,
                 collect._toDelete.begin(), collect._toDelete.end() ) << endl;
    }

  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::instance().logNothing();
  return 0;
}

