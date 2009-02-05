#include "Tools.h"
#include "zypp/pool/GetResolvablesToInsDel.h"

#include "zypp/parser/plaindir/RepoParser.h"

static TestSetup test( "/tmp/ToolScanRepos", Arch_x86_64 );  // use x86_64 as system arch
//static TestSetup test( Arch_x86_64 );  // use x86_64 as system arch

int main( int argc, char * argv[] )
try {
  --argc;
  ++argv;
  zypp::base::LogControl::instance().logToStdErr();
  INT << "===[START]==========================================" << endl;

  test.loadRepos();
  ResPool pool( test.pool() );

  static const sat::SolvAttr susetagsDatadir( "susetags:datadir" );
  sat::LookupRepoAttr q( susetagsDatadir );
  dumpRange(SEC << q << " ", q.begin(), q.end() ) << endl;

  for_( it, pool.byKindBegin<Package>(), pool.byKindEnd<Package>() )
  {
    ManagedFile p( providePkg( *it ) );
    PathInfo pi( p );
    (pi.isFile() ? USR : INT) << pi << endl;
    break;
  }


  ///////////////////////////////////////////////////////////////////
  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::instance().logNothing();
  return 0;
}
catch ( const Exception & exp )
{
  INT << exp << endl << exp.historyAsString();
}
catch (...)
{}

