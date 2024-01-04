#include <sstream>
#include <string>
#include <zypp/parser/RepoFileReader.h>
#include <zypp/base/NonCopyable.h>

#include "TestSetup.h"

using std::stringstream;
using std::string;
using namespace zypp;

// This is kind of ugly for testing: Whenever a zypp lock is
// acquired the repo variables are cleared and re-read from disk,
// which is ok (by ZYppFactory).
// The VarReplacer itself however acquires a lock to resolve the
// Target variables (like $releasever), which is ok as well.
// For testing however - if we want to manipulate the variable
// definitions - we must hold a lock ourselves. Otherwise the
// VarReplacer will acquired one and so discard our custom
// settings.
#include <zypp/ZYppFactory.h>
auto guard { getZYpp() };
namespace zyppintern {
  std::map<std::string,std::string> repoVariablesGet();
  void repoVariablesSwap( std::map<std::string,std::string> & val_r );
}
// ---

static std::string suse_repo = "[factory-oss]\n"
"name=factory-oss $releasever - $basearch\n"
"failovermethod=priority\n"
"enabled=1\n"
"gpgcheck=1\n"
"autorefresh=0\n"
"baseurl=http://serv.er/loc1\n"
"baseurl=http://serv.er/loc2\n"
"        http://serv.er/loc3  ,   http://serv.er/loc4     http://serv.er/loc5\n"
"gpgkey=http://serv.er/loc1\n"
"gpgkey=http://serv.er/loc2\n"
"        http://serv.er/loc3  ,   http://serv.er/loc4     http://serv.er/loc5\n"
"mirrorlist=http://serv.er/loc1\n"
"mirrorlist=http://serv.er/loc2\n"
"        http://serv.er/loc3  ,   http://serv.er/loc4     http://serv.er/loc5\n"
"type=NONE\n"
"keeppackages=0\n";

struct RepoCollector : private base::NonCopyable
{
  bool collect( const RepoInfo &repo )
  {
    repos.push_back(repo);
    return true;
  }

  RepoInfoList repos;
};

// Must be the first test!
BOOST_AUTO_TEST_CASE(read_repo_file)
{
  {
    std::stringstream input(suse_repo);
    RepoCollector collector;
    parser::RepoFileReader parser( input, bind( &RepoCollector::collect, &collector, _1 ) );
    BOOST_CHECK_EQUAL(1, collector.repos.size());

    const RepoInfo & repo( collector.repos.front() );

    BOOST_CHECK_EQUAL( 5, repo.baseUrlsSize() );
    BOOST_CHECK_EQUAL( 5, repo.gpgKeyUrlsSize() );
    BOOST_CHECK_EQUAL( Url("http://serv.er/loc1"), repo.url() );
    BOOST_CHECK_EQUAL( Url("http://serv.er/loc1"), repo.gpgKeyUrl() );
    BOOST_CHECK_EQUAL( Url("http://serv.er/loc1"), repo.mirrorListUrl() );
  }
}

BOOST_AUTO_TEST_CASE(rawurl2repoinfo)
{
  // Set up repo variables...
  std::map<std::string,std::string> vardef { ::zyppintern::repoVariablesGet() };
  vardef["releasever"]       = "myversion";
  vardef["basearch"]         = "myarch";
  vardef["OPENSUSE_DISTURL"] = "https://cdn.opensuse.org/repositories/";
  ::zyppintern::repoVariablesSwap( vardef );

  {
    std::stringstream input(
      "[leap-repo]\n"
      "name=leap-repo $releasever - $basearch\n"
      "baseurl=    ${OPENSUSE_DISTURL}leap/repo\n"
      "gpgkey=     ${OPENSUSE_DISTURL}leap/repo\n"
      "mirrorlist= ${OPENSUSE_DISTURL}leap/repo\n"
      "metalink=   ${OPENSUSE_DISTURL}leap/repo\n"
    );
    RepoCollector collector;
    parser::RepoFileReader parser( input, bind( &RepoCollector::collect, &collector, _1 ) );
    BOOST_CHECK_EQUAL(1, collector.repos.size());

    const RepoInfo & repo( collector.repos.front() );
    BOOST_CHECK_EQUAL( repo.name(), "leap-repo myversion - myarch"  );

    BOOST_CHECK_EQUAL( repo.url().asCompleteString(),           "https://cdn.opensuse.org/repositories/leap/repo" );
    BOOST_CHECK_EQUAL( repo.gpgKeyUrl().asCompleteString(),     "https://cdn.opensuse.org/repositories/leap/repo" );
    BOOST_CHECK_EQUAL( repo.mirrorListUrl().asCompleteString(), "https://cdn.opensuse.org/repositories/leap/repo" );

    BOOST_CHECK_EQUAL( repo.rawUrl().asCompleteString(),           "zypp-rawurl:#${OPENSUSE_DISTURL}leap/repo" );
    BOOST_CHECK_EQUAL( repo.rawGpgKeyUrl().asCompleteString(),     "zypp-rawurl:#${OPENSUSE_DISTURL}leap/repo" );
    BOOST_CHECK_EQUAL( repo.rawMirrorListUrl().asCompleteString(), "zypp-rawurl:#${OPENSUSE_DISTURL}leap/repo" );
  }

  // RepoFileReader unfortunately encodes "proxy=" into the URLs
  // query part (by now just for the baseurl). For RawUrls the
  // VarReplacer needs to take this into account.
  {
    std::stringstream input(
      "[leap-repo]\n"
      "name=leap-repo $releasever - $basearch\n"
      "proxy=myproxy.host:1234\n"
      "baseurl=${OPENSUSE_DISTURL}leap/repo\n"
      "baseurl=https://cdn.opensuse.org/repositories/leap/repo\n"
      "baseurl=https://cdn.opensuse.org/repositories/leap/repo?proxy=otherproxy.host\n"
      "gpgkey=${OPENSUSE_DISTURL}leap/repo\n"
      "mirrorlist=${OPENSUSE_DISTURL}leap/repo\n"
      "metalink=${OPENSUSE_DISTURL}leap/repo\n"
    );
    RepoCollector collector;
    parser::RepoFileReader parser( input, bind( &RepoCollector::collect, &collector, _1 ) );
    BOOST_CHECK_EQUAL(1, collector.repos.size());

    const RepoInfo & repo( collector.repos.front() );
    BOOST_CHECK_EQUAL( repo.name(), "leap-repo myversion - myarch"  );

    BOOST_CHECK_EQUAL( repo.baseUrlsSize(), 3 );
    const auto & baseurls = repo.baseUrls();
    auto iter = baseurls.begin();
    BOOST_CHECK_EQUAL( (iter++)->asCompleteString(), "https://cdn.opensuse.org/repositories/leap/repo?proxy=myproxy.host&proxyport=1234" );
    BOOST_CHECK_EQUAL( (iter++)->asCompleteString(), "https://cdn.opensuse.org/repositories/leap/repo?proxy=myproxy.host&proxyport=1234" );
    BOOST_CHECK_EQUAL( (iter++)->asCompleteString(), "https://cdn.opensuse.org/repositories/leap/repo?proxy=otherproxy.host" );
    BOOST_CHECK_EQUAL( repo.gpgKeyUrl().asCompleteString(),     "https://cdn.opensuse.org/repositories/leap/repo" );
    BOOST_CHECK_EQUAL( repo.mirrorListUrl().asCompleteString(), "https://cdn.opensuse.org/repositories/leap/repo" );

    const auto & rawbaseurls = repo.rawBaseUrls();
    iter = rawbaseurls.begin();
    BOOST_CHECK_EQUAL( (iter++)->asCompleteString(), "zypp-rawurl:?proxy=myproxy.host&proxyport=1234#${OPENSUSE_DISTURL}leap/repo" );
    BOOST_CHECK_EQUAL( (iter++)->asCompleteString(), "https://cdn.opensuse.org/repositories/leap/repo?proxy=myproxy.host&proxyport=1234" );
    BOOST_CHECK_EQUAL( (iter++)->asCompleteString(), "https://cdn.opensuse.org/repositories/leap/repo?proxy=otherproxy.host" );
    BOOST_CHECK_EQUAL( repo.rawGpgKeyUrl().asCompleteString(),     "zypp-rawurl:#${OPENSUSE_DISTURL}leap/repo" );
    BOOST_CHECK_EQUAL( repo.rawMirrorListUrl().asCompleteString(), "zypp-rawurl:#${OPENSUSE_DISTURL}leap/repo" );
  }
}
