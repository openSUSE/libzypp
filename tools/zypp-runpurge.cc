#include <zypp/PurgeKernels.h>
#include "argparse.h"

#define INCLUDE_TESTSETUP_WITHOUT_BOOST
#include "../tests/lib/TestSetup.h"
#undef  INCLUDE_TESTSETUP_WITHOUT_BOOST

static std::string appname { "NO_NAME" };

int errexit( const std::string & msg_r = std::string(), int exit_r = 100 )
{
  if ( ! msg_r.empty() )
    cerr << endl << appname << ": ERR: " << msg_r << endl << endl;
  return exit_r;
}

int usage( const argparse::Options & options_r, int return_r = 0 )
{
  cerr << "USAGE: " << appname << " [OPTION]... path/to/testcase" << endl;
  cerr << "    Calculate the kernels that would be purged based on the spec and testcase." << endl;
  cerr << options_r << endl;
  return return_r;
}

int main ( int argc, char *argv[] )
{

  appname = Pathname::basename( argv[0] );
  argparse::Options options;
  options.add()
    ( "help,h",   "Print help and exit." )
    ( "uname",    "The running kernels uname", argparse::Option::Arg::required )
    ( "keepSpec", "The keepspec ( default is oldest,running,latest)", argparse::Option::Arg::required );

  auto result = options.parse( argc, argv );

  if ( result.count( "help" ) || !result.positionals().size() )
    return usage( options, 1 );

  const std::string &testcaseDir = result.positionals().front();
  const auto &pathInfo = PathInfo(testcaseDir);
  if ( !pathInfo.isExist() || !pathInfo.isDir() ) {
    std::cerr << "Invalid or non existing testcase path: " << testcaseDir << std::endl;
    return 1;
  }

  if ( ::chdir( testcaseDir.data() ) != 0 ) {
    std::cerr << "Failed to chdir to " << testcaseDir << std::endl;
    return 1;
  }

  TestSetup t;
  try {
    t.LoadSystemAt( "." );
  }  catch ( const zypp::Exception &e ) {
    std::cerr << "Failed to load the testcase at " << result.positionals().front() << std::endl;
    std::cerr << "Got exception: " << e << std::endl;
    return 1;
  }

  std::string unameR;
  if ( result.count("uname") ) {
    unameR = result["uname"].arg();
  } else {
    std::cout << "No --uname provided. Guessing it from the latest kernel-default installed...." << std::endl;
    const PoolItem & running( ui::Selectable::get("kernel-default")->theObj() );
    if ( not running ) {
      std::cerr << "Oops: No installed kernel-default." << std::endl;
      return usage( options, 1 );
    }
    std::cout << "Guess running: " << running.asString() << endl;
    const Capability unameRProvides { "kernel-uname-r" }; // indicator provides
    for ( const auto & cap : running.provides() ) {
      if ( cap.matches( unameRProvides ) == CapMatch::yes ) {
        unameR = cap.detail().ed().asString();
        break;
      }
    }
    if ( unameR.empty() ) {
      std::cerr << "Oops: Guessed kernel does not provide " << unameRProvides << std::endl;
      return usage( options, 1 );
    }
    std::cout << "Guess --uname: " << unameR << endl;
  }

  std::string keepSpec = "oldest,running,latest";
  if ( result.count("keepSpec") ) {
    keepSpec = result["keepSpec"].arg();
  }


  PurgeKernels krnls;
  krnls.setUnameR( unameR );
  krnls.setKeepSpec( keepSpec );
  krnls.markObsoleteKernels();


  std::cout << "Purged kernels: " << std::endl;
  auto pool = ResPool::instance();
  const filter::ByStatus toBeUninstalledFilter( &ResStatus::isToBeUninstalled );
  for ( auto it = pool.byStatusBegin( toBeUninstalledFilter ); it != pool.byStatusEnd( toBeUninstalledFilter );  it++  ) {
    std::cout << "Removing " << it->asString() + (it->status().isByUser() ? " (by user)" : " (autoremoved)") << std::endl;
  }

  return 0;

}
