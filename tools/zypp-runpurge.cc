#include <zypp/PoolQuery.h>
#include <zypp/sat/WhatProvides.h>
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


// Helper for version sorted containers
struct NVR : public sat::Solvable {
  NVR( const sat::Solvable & slv )
  : sat::Solvable { slv }
  {}
};

inline std::ostream & operator<<( std::ostream & str, const NVR & obj )
{ return str << obj.ident() << " " << obj.edition() << (obj.isSystem()?" (i)":""); }

inline bool operator<( const NVR & lhs, const NVR & rhs )
{ return compareByNVRA( lhs, rhs ) < 0; }


// Cluster KMPs by kernel and fitting KMP versions (check either installed or available versions)
// (see also bsc#1232399 etc.)
void KMPcheck( const bool checkSystem = true )
{
  using SolvableSet =std::unordered_set<sat::Solvable>;

  // Whether slv is on container
  auto contains = []( const SolvableSet & container, const sat::Solvable & slv ) -> bool {
    return container.find( slv ) != container.end();
  };

  // Remove items from fitting which do not occur in matching
  auto intersect = [&]( SolvableSet & fitting, const SolvableSet & matching ) {
    for ( auto it = fitting.begin(); it != fitting.end(); ) {
      if ( contains( matching, *it ) )
        ++it;
      else
        it = fitting.erase( it );
    }
  };

  SolvableSet kernels;  // multiversion kernels
  SolvableSet kmps;     // multiversion kmps
  SolvableSet others;   // multiversion others
  {
    const Capability idxcap { "kernel-uname-r" };
    sat::WhatProvides idx { idxcap };
    for ( const auto & i : idx ) {
      if ( i.isSystem() != checkSystem )
        continue;
      kernels.insert( i );
    }
  }
  {
    const Capability idxcap { "multiversion(kernel)" };
    sat::WhatProvides idx { idxcap };

    StrMatcher matchMod { "kmod(*)", Match::GLOB };
    auto isKmp = [&matchMod]( const sat::Solvable & slv ) -> bool {
      for ( const auto & prov : slv.provides() ) {
        if ( matchMod.doMatch( prov.detail().name().c_str() ) )
          return true;
      }
      return false;
    };

    for ( const auto & i : idx ) {
      if ( i.isSystem() != checkSystem )
        continue;
      if ( not contains( kernels, i ) ) {
        ( isKmp( i ) ? kmps : others ).insert( i );
      }
    }
  }
  cout << "K " << kernels.size() << ", M " << kmps.size() << ", O " << others.size() << endl;

  // Caching which KMP requirement is provided by what kernels.
  std::unordered_map<Capability,SolvableSet> whatKernelProvidesCache;
  auto whatKernelProvides = [&]( const Capability & req ) -> const SolvableSet & {
    auto iter = whatKernelProvidesCache.find( req );
    if ( iter != whatKernelProvidesCache.end() )
      return iter->second;  // hit
    // miss:
    SolvableSet & providingKernels = whatKernelProvidesCache[req];
    sat::WhatProvides idx { req };
    for ( const auto & i : idx ) {
      if ( contains( kernels, i ) )
        providingKernels.insert( i );
    }
    return providingKernels;
  };

#if 0
  using Bucket     = SolvableSet;                               // fitting KMP versions
  using KmpBuckets = std::unordered_map<sat::Solvable,Bucket>;  // kernel : fitting KMP versions
  using KernelKmps = std::unordered_map<IdString,KmpBuckets>;   // KMP name : kernel : fitting KMP versions
#else
  // sorted containers:
  using Bucket     = std::set<NVR>;                 // fitting KMP versions
  using KmpBuckets = std::map<NVR,Bucket>;          // kernel : fitting KMP versions
  using KernelKmps = std::map<IdString,KmpBuckets>; // KMP name : kernel : fitting KMP versions
#endif
  KernelKmps kernelKmps;

  // Cluster the KMPs....
  for ( const auto & kmp : kmps ) {
    //cout << endl << "==== " << kmp << endl;
    std::optional<SolvableSet> fittingKernels; // kernel satisfying all kernel related requirements
    for ( const auto & req : kmp.requires() ) {
      const SolvableSet & providingKernels { whatKernelProvides( req ) };
      if ( providingKernels.size() == 0 )
        continue; // a not kernel related requirement
      if ( not fittingKernels ) {
        fittingKernels = providingKernels; // initial set
      } else {
        intersect( *fittingKernels, providingKernels );
      }
      //cout << "?? " << req << " provided by " << providingKernels.size() << endl;
      //cout << " - fit " << fittingKernels->size() << endl;
    }
    if ( fittingKernels ) {
      for ( const auto & kernel : *fittingKernels ) {
        kernelKmps[kmp.ident()][kernel].insert( kmp );
      }
    }
    else {
      cout << endl << "==== " << kmp << endl;
      cout << "     No fitting kernels!" << endl;
    }
  }

  // Print the clusters...
  if ( checkSystem ) {

    // For the installed/prunned system cluster per kernel is IMO easier to check
    // ==== kernel-default 5.14.21-150400.24.136.1 (i)
    //      drbd-kmp-default fit 3 versions(s)
    //          - drbd-kmp-default 9.0.30~1+git.10bee2d5_k5.14.21_150400.22-150400.1.75 (i)
    //          - drbd-kmp-default 9.0.30~1+git.10bee2d5_k5.14.21_150400.24.11-150400.3.2.9 (i)
    //          - drbd-kmp-default 9.0.30~1+git.10bee2d5_k5.14.21_150400.24.46-150400.3.4.1 (i)
    std::map<NVR,std::map<IdString,Bucket>> reordered;
    for ( const auto & [kmp,bucket] : kernelKmps ) {
      for ( const auto & [kernel,versions] : bucket ) {
        reordered[kernel][kmp] = versions;
      }
    }
    for ( const auto & [kernel,bucket] : reordered ) {
      cout << endl << "==== " << kernel << endl;
      for ( const auto & [kmp,versions] : bucket ) {
        cout << "     " << kmp << " fit " << versions.size() << " versions(s)" << endl;
        for ( const auto & version : versions )
          cout << "         - " << version << endl;
      }
    }

  } else {

    // ==== drbd-kmp-default
    //      ABI kernel-default 5.14.21-150400.24.136.1 (i)
    //      fit 3 versions(s)
    //          - drbd-kmp-default 9.0.30~1+git.10bee2d5_k5.14.21_150400.22-150400.1.75 (i)
    //          - drbd-kmp-default 9.0.30~1+git.10bee2d5_k5.14.21_150400.24.11-150400.3.2.9 (i)
    //          - drbd-kmp-default 9.0.30~1+git.10bee2d5_k5.14.21_150400.24.46-150400.3.4.1 (i)
    for ( const auto & [kmp,bucket] : kernelKmps ) {
      cout << endl << "==== " << kmp << endl;
      const Bucket * lastBucket = nullptr;
      for ( const auto & [kernel,versions] : bucket ) {
        if ( lastBucket && *lastBucket != versions ) {
          cout << "     fit " << lastBucket->size() << " versions(s)" << endl;
          for ( const auto & version : *lastBucket )
            cout << "         - " << version << endl;
        }
        cout << "     ABI " << kernel << endl;
        lastBucket = &versions;
      }
      if ( lastBucket ) {
        cout << "     fit " << lastBucket->size() << " versions(s)" << endl;
        for ( const auto & version : *lastBucket )
          cout << "         - " << version << endl;
      }
    }

  }
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

  std::cout << "KMPcheck: " << std::endl;
  KMPcheck();

  return 0;

}
