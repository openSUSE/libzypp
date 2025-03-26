#include "zypp/PoolItemBest.h"
#define INCLUDE_TESTSETUP_WITHOUT_BOOST
#include "../../tests/lib/TestSetup.h"
#undef  INCLUDE_TESTSETUP_WITHOUT_BOOST

#include "../../tools/argparse.h"
#include <zypp/PoolQuery.h>
#include <iostream>

static std::string appname { "testrunner" };

struct KeyRingReceiver : public callback::ReceiveReport<KeyRingReport>
{
  KeyRingReceiver() {
    connect();
  }
  virtual KeyTrust askUserToAcceptKey( const PublicKey &key, const KeyContext &keycontext )
  {
    std::cout << "Trusting key: " << key << std::endl;
    return KEY_TRUST_AND_IMPORT;
  }
};

int errexit( const std::string & msg_r = std::string(), int exit_r = 100 )
{
  if ( ! msg_r.empty() )
    cerr << endl << appname << ": ERR: " << msg_r << endl << endl;
  return exit_r;
}


int usage( const argparse::Options & options_r, int return_r = 0 )
{
  cerr << "USAGE: " << appname << " [OPTION]... path/to/testcase" << endl;
  cerr << "    Runs the given testcase." << endl;
  cerr << options_r << endl;
  return return_r;
}

int main ( int argc, char *argv[] )
{

  appname = Pathname::basename( argv[0] );
  argparse::Options options;
  options.add()
    ( "help,h",   "Print help and exit." )
    ( "root,r",   "Use the system at the given path. ( default / )", argparse::Option::Arg::required  );


  auto result = options.parse( argc, argv );

  if ( result.count( "help" ) )
    return usage( options, 1 );

  Pathname sysRoot = "/";
  if ( result.count("root") ) {
    auto arg = result["root"];
    sysRoot = arg.arg();
  }

  std::cout << "Using system at: " << sysRoot << std::endl;


  ZYpp::Ptr zypp = getZYpp();		// acquire initial zypp lock

  KeyRingReceiver accKeyHelper; // auto accepting key2

  sat::Pool satpool( sat::Pool::instance() );
  USR << "*** load target '" << Repository::systemRepoAlias() << "'\t" << endl;
  getZYpp()->initializeTarget( sysRoot );
  getZYpp()->target()->load();
  USR << satpool.systemRepo() << endl;

  {
    RepoManager repoManager( sysRoot );

    // sync the current repo set
    for ( RepoInfo & nrepo : repoManager.knownRepositories() )
    {
      if ( ! nrepo.enabled() )
        continue;

      // Often volatile media are sipped in automated environments
      // to avoid media chagne requests:
      if ( nrepo.url().schemeIsVolatile() )
        continue;

      bool refreshNeeded = false;
      if ( nrepo.autorefresh() )	// test whether to autorefresh repo metadata
      {
        for ( const Url & url : nrepo.baseUrls() )
        {
          try
          {
            if ( repoManager.checkIfToRefreshMetadata( nrepo, url ) == RepoManager::REFRESH_NEEDED )
            {
              cout << "Need to autorefresh repo " << nrepo.alias() << endl;
              refreshNeeded = true;
            }
            break;	// exit after first successful checkIfToRefreshMetadata
          }
          catch ( const Exception & exp )
          {}	// Url failed, try next one...
        }
      }

      // initial metadata download or cache refresh
      if ( ! repoManager.isCached( nrepo ) || refreshNeeded )
      {
        cout << "Refreshing repo " << nrepo << endl;
        if ( repoManager.isCached( nrepo ) )
        {
          repoManager.cleanCache( nrepo );
        }
        repoManager.refreshMetadata( nrepo );
        repoManager.buildCache( nrepo );
      }

      // load cache
      try
      {
        cout << "Loading resolvables from " << nrepo.alias() << endl;
        repoManager.loadFromCache( nrepo );// load available packages to pool
      }
      catch ( const Exception & exp )
      {
        // cachefile has old fomat (or is corrupted): try to rebuild it
        repoManager.cleanCache( nrepo );
        repoManager.buildCache( nrepo );
        repoManager.loadFromCache( nrepo );
      }
    }
  }

  cout << zypp->pool() << endl;
  cout << "=====[pool ready]==============================" << endl;


  PoolQuery q;
  q.addKind( zypp::ResKind::package );
  q.addAttribute(sat::SolvAttr::name, "ptest");
  q.setUninstalledOnly();
  q.setMatchExact();

  PoolItemBest bestMatches( q.begin(), q.end() );
  if ( ! bestMatches.empty() ) {
    for_( it, bestMatches.begin(), bestMatches.end() ) {
      ui::asSelectable()( *it )->setOnSystem( *it, ResStatus::USER );
    }
  }

  {
    cout << "Solving dependencies..." << endl;

    unsigned attempt = 0;
    while ( ! zypp->resolver()->resolvePool() ) {
      ++attempt;
      cout << "Solving dependencies: " << attempt << ". attempt failed" << endl;
      const ResolverProblemList & problems( zypp->resolver()->problems() );
      cout << problems.size() << " problems found..." << endl;

      unsigned probNo = 0;
      for ( const auto & probPtr : problems ) {

        cout << "Problem " << ++probNo << ": " << probPtr->description() << endl;

        const ProblemSolutionList & solutions = probPtr->solutions();
        unsigned solNo = 0;
        for ( const auto & solPtr : solutions ) {

          cout << "  Solution " << ++solNo << ": " << solPtr->description() << endl;
        }
      }

      return errexit ( "Failed to solve dependencies" );
    }
    cout << "Dependencies solved" << endl;
  }

  return 0;
}
