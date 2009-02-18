#include "Tools.h"
#include <zypp/ResObjects.h>

#include <zypp/sat/LookupAttr.h>
#include <zypp/sat/WhatProvides.h>
#include <zypp/sat/WhatObsoletes.h>

#include <zypp/PoolQuery.h>
#include <zypp/ui/Selectable.h>

using namespace zypp;

/******************************************************************
**
**      FUNCTION NAME : main
**      FUNCTION TYPE : int
*/
int main( int argc, char * argv[] )
{
  std::string appname( Pathname::basename( argv[0] ) ); --argc, ++argv;
  INT << "===[START]==========================================" << endl;

  TestSetup test( "/tmp/"+appname, Arch_x86_64, TSO_CLEANROOT );
  test.loadRepo( "http://download.opensuse.org/repositories/Banshee/openSUSE_11.1", "banshee-1.4" );

  ResPool pool( test.pool() );
  ResPoolProxy poolProxy( test.poolProxy() );
  //dumpRange( MIL, pool.byKindBegin<Package>(), pool.byKindEnd<Package>() ) << endl;
  for_( it, pool.byKindBegin<Pattern>(), pool.byKindEnd<Pattern>() )
  {
    MIL << *it << endl;
    DBG << ui::Selectable::get( *it ) << endl;
  }
  for_( it, poolProxy.byKindBegin<Pattern>(), poolProxy.byKindEnd<Pattern>() )
  {
    MIL << *it << endl;
  }

  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::TmpLineWriter shutUp;
  return 0;
}

