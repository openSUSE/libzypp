/*
** Check if the url by scheme repository works, e.g.
** if there are some initialization order problems
** (ViewOption) causing asString to format its string
** differently than configured.
*/

#include <zypp/base/Exception.h>
#include <zypp/base/String.h>

#include <zypp/RepoInfo.h>

#include <zypp/Url.h>
#include <stdexcept>
#include <iostream>
#include <cassert>

// Boost.Test
#include <boost/test/unit_test.hpp>

using boost::unit_test::test_case;
using namespace zypp;

void testUrlAuthority( const Url & url_r,
                       const std::string & host_r, const std::string & port_r = std::string(),
                       const std::string & user_r = std::string(), const std::string & pass_r = std::string() )
{
  BOOST_CHECK_EQUAL( url_r.getUsername(),	user_r );
  BOOST_CHECK_EQUAL( url_r.getPassword(),	pass_r );
  BOOST_CHECK_EQUAL( url_r.getHost(),		host_r );
  BOOST_CHECK_EQUAL( url_r.getPort(),		port_r );
}


BOOST_AUTO_TEST_CASE(test_ipv6_url)
{
    std::string str;
    zypp::Url   url;

    str = "http://[2001:DB8:0:F102::1]/64/sles11/RC1/CD1?device=eth0";
    url = Url( str );
    BOOST_CHECK_EQUAL( str,url.asString() );
    testUrlAuthority( url, "[2001:DB8:0:F102::1]", "", "", "" );

    // bnc#
    str = "http://[2001:DB8:0:F102::1]:8080/64/sles11/RC1/CD1?device=eth0";
    url = Url( str );
    testUrlAuthority( url, "[2001:DB8:0:F102::1]", "8080", "", "" );


    str = "http://user:pass@[2001:DB8:0:F102::1]:8080/64/sles11/RC1/CD1?device=eth0";
    url = Url( str );
    testUrlAuthority( url, "[2001:DB8:0:F102::1]", "8080", "user", "pass" );
}

BOOST_AUTO_TEST_CASE(test_url1)
{
    std::string str, one, two;
    zypp::Url   url;


    // asString & asCompleteString should not print "mailto://"
    str = "mailto:feedback@example.com?subject=hello";
    url = str;
    BOOST_CHECK_EQUAL( str, url.asString() );
    BOOST_CHECK_EQUAL( str, url.asCompleteString() );

    // In general, schema without authority allows specifying an empty authority
    // though it should not be printed (unless explicitly requested).
    BOOST_CHECK_EQUAL( Url("dvd:/srv/ftp").asCompleteString(),   "dvd:/srv/ftp" );
    BOOST_CHECK_EQUAL( Url("dvd:/srv/ftp").asString(),           "dvd:/srv/ftp" );

    BOOST_CHECK_EQUAL( Url("dvd:///srv/ftp").asCompleteString(), "dvd:/srv/ftp" );
    BOOST_CHECK_EQUAL( Url("dvd:///srv/ftp").asString(),         "dvd:/srv/ftp" );

    BOOST_CHECK_EQUAL( Url("dvd:///srv/ftp").asString(url::ViewOption::DEFAULTS+url::ViewOption::EMPTY_AUTHORITY),        "dvd:///srv/ftp" );
    BOOST_CHECK_EQUAL( Url("dvd:///srv/ftp").asString(url::ViewOption::DEFAULTS-url::ViewOption::EMPTY_AUTHORITY),        "dvd:/srv/ftp" );

    BOOST_CHECK_THROW( Url("dvd://authority/srv/ftp"), url::UrlNotAllowedException );

    // asString shouldn't print the password, asCompleteString should
    // further, the "//" at the begin of the path should become "/%2F"
    str = "ftp://user:pass@localhost//srv/ftp";
    one = "ftp://user@localhost/%2Fsrv/ftp";
    two = "ftp://user:pass@localhost/%2Fsrv/ftp";
    url = str;

    BOOST_CHECK_EQUAL( one, url.asString() );
    BOOST_CHECK_EQUAL( two, url.asCompleteString() );

    // asString shouldn't print the password, asCompleteString should.
    // further, the "//" at the begin of the path should be keept.
    str = "http://user:pass@localhost//srv/ftp?proxypass=@PROXYPASS@&proxy=proxy.my&proxyuser=@PROXYUSER@&Xproxypass=NOTTHIS&proxypass=@PROXYPASS@&proxypass=@PROXYPASS@";
    one = "http://user@localhost//srv/ftp?proxy=proxy.my&proxyuser=@PROXYUSER@&Xproxypass=NOTTHIS";
    two = str;
    url = str;

    BOOST_CHECK_EQUAL( one, url.asString() );
    BOOST_CHECK_EQUAL( two, url.asCompleteString() );
    // hidden proxypass in the query is available when explicitly asked for
    BOOST_CHECK_EQUAL( url.getQueryParam( "proxypass" ), "@PROXYPASS@" );

    // absolute path defaults to 'file://'
    str = "/some/local/path";
    BOOST_CHECK_EQUAL( zypp::Url(str).asString(), "file://"+str );

    str = "file:./srv/ftp";
    BOOST_CHECK_EQUAL( zypp::Url(str).asString(), str );

    str = "ftp://foo//srv/ftp";
    BOOST_CHECK_EQUAL( zypp::Url(str).asString(), "ftp://foo/%2Fsrv/ftp" );

    str = "FTP://user@local%68ost/%2f/srv/ftp";
    BOOST_CHECK_EQUAL( zypp::Url(str).asString(), "ftp://user@localhost/%2f/srv/ftp" );

    str = "http://[::1]/foo/bar";
    BOOST_CHECK_EQUAL( str, zypp::Url(str).asString() );

    str = "http://:@just-localhost.example.net:8080/";
    BOOST_CHECK_EQUAL( zypp::Url(str).asString(), "http://just-localhost.example.net:8080/" );

    str = "mailto:feedback@example.com?subject=hello";
    BOOST_CHECK_EQUAL( str, zypp::Url(str).asString() );

    str = "nfs://nfs-server/foo/bar/trala";
    BOOST_CHECK_EQUAL( str, zypp::Url(str).asString() );

    str = "ldap://example.net/dc=example,dc=net?cn,sn?sub?(cn=*)#x";
    BOOST_CHECK_THROW( zypp::Url(str).asString(), url::UrlNotAllowedException );

    str = "ldap://example.net/dc=example,dc=net?cn,sn?sub?(cn=*)";
    BOOST_CHECK_EQUAL( str, zypp::Url(str).asString() );

    // parseable but invalid, since no host avaliable
    str = "ldap:///dc=foo,dc=bar";
    BOOST_CHECK_EQUAL( str, zypp::Url(str).asString());
    BOOST_CHECK( !zypp::Url(str).isValid());

    // throws:  host is mandatory
    str = "ftp:///foo/bar";
    BOOST_CHECK_THROW(zypp::Url(str).asString(), url::UrlNotAllowedException );

    // throws:  host is mandatory
    str = "http:///%2f/srv/ftp";
    BOOST_CHECK_THROW(zypp::Url(str).asString(), url::UrlNotAllowedException );

    // OK, host allowed in file-url
    str = "file://localhost/some/path";
    BOOST_CHECK_EQUAL( str, zypp::Url(str).asString());

    // throws:  host not allowed
    str = "cd://localhost/some/path";
    BOOST_CHECK_THROW(zypp::Url(str).asString(), url::UrlNotAllowedException );

    // throws: no path (email)
    str = "mailto:";
    BOOST_CHECK_THROW(zypp::Url(str).asString(), url::UrlNotAllowedException );

    // throws:  no path
    str = "cd:";
    BOOST_CHECK_THROW(zypp::Url(str).asString(), url::UrlNotAllowedException );

    // OK, valid (no host, path is there)
    str = "cd:///some/path";
    BOOST_CHECK( zypp::Url(str).isValid());
}

BOOST_AUTO_TEST_CASE(test_url2)
{
  zypp::Url url("http://user:pass@localhost:/path/to;version=1.1?arg=val#frag");

  BOOST_CHECK_EQUAL( url.asString(),
  "http://user@localhost/path/to?arg=val#frag" );

  BOOST_CHECK_EQUAL( url.asString(zypp::url::ViewOptions() +
                     zypp::url::ViewOptions::WITH_PASSWORD),
  "http://user:pass@localhost/path/to?arg=val#frag");

  BOOST_CHECK_EQUAL( url.asString(zypp::url::ViewOptions() +
                     zypp::url::ViewOptions::WITH_PATH_PARAMS),
  "http://user@localhost/path/to;version=1.1?arg=val#frag");

  BOOST_CHECK_EQUAL( url.asCompleteString(),
  "http://user:pass@localhost/path/to;version=1.1?arg=val#frag");
}

BOOST_AUTO_TEST_CASE(test_url3)
{
  zypp::Url   url("http://localhost/path/to#frag");
  std::string key;
  std::string val;

  // will be encoded as "hoho=ha%20ha"
  key = "hoho";
  val = "ha ha";
  url.setQueryParam(key, val);
  BOOST_CHECK_EQUAL( url.asString(),
  "http://localhost/path/to?hoho=ha%20ha#frag");

  // will be encoded as "foo%3Dbar%26key=foo%26bar=value"
  // bsc#1234304: `=` must be encoded in key but is a safe char in value
  key = "foo=bar&key";
  val = "foo&bar=value";
  url.setQueryParam(key, val);
  BOOST_CHECK_EQUAL( url.asString(),
  "http://localhost/path/to?foo%3Dbar%26key=foo%26bar=value&hoho=ha%20ha#frag");

  // will be encoded as "foo%25bar=is%25de%25ad"
  key = "foo%bar";
  val = "is%de%ad";
  url.setQueryParam(key, val);
  BOOST_CHECK_EQUAL( url.asString(),
  "http://localhost/path/to?foo%25bar=is%25de%25ad&foo%3Dbar%26key=foo%26bar=value&hoho=ha%20ha#frag");

  // get encoded query parameters and compare with results:
  zypp::url::ParamVec params( url.getQueryStringVec());
  const char * const  result[] = {
    "foo%25bar=is%25de%25ad",
    "foo%3Dbar%26key=foo%26bar=value",
    "hoho=ha%20ha"
  };
  BOOST_CHECK( params.size() == (sizeof(result)/sizeof(result[0])));
  for( size_t i=0; i<params.size(); i++)
  {
      BOOST_CHECK_EQUAL( params[i], result[i]);
  }
}

BOOST_AUTO_TEST_CASE( test_url4)
{
  try
  {
    zypp::Url url("ldap://example.net/dc=example,dc=net?cn,sn?sub?(cn=*)");

    // fetch query params as vector
    zypp::url::ParamVec pvec( url.getQueryStringVec());
    BOOST_CHECK( pvec.size() == 3);
    BOOST_CHECK_EQUAL( pvec[0], "cn,sn");
    BOOST_CHECK_EQUAL( pvec[1], "sub");
    BOOST_CHECK_EQUAL( pvec[2], "(cn=*)");

    // fetch the query params map
    // with its special ldap names/keys
    zypp::url::ParamMap pmap( url.getQueryStringMap());
    zypp::url::ParamMap::const_iterator m;
    for(m=pmap.begin(); m!=pmap.end(); ++m)
    {
      if("attrs"  == m->first)
      {
        BOOST_CHECK_EQUAL( m->second, "cn,sn");
      }
      else
      if("filter" == m->first)
      {
        BOOST_CHECK_EQUAL( m->second, "(cn=*)");
      }
      else
      if("scope"  == m->first)
      {
        BOOST_CHECK_EQUAL( m->second, "sub");
      }
      else
      {
        BOOST_FAIL("Unexpected LDAP query parameter name in the map!");
      }
    }

    url.setQueryParam("attrs", "cn,sn,uid");
    url.setQueryParam("filter", "(|(sn=foo)(cn=bar))");

    BOOST_CHECK_EQUAL(url.getQueryParam("attrs"),  "cn,sn,uid");
    BOOST_CHECK_EQUAL(url.getQueryParam("filter"), "(|(sn=foo)(cn=bar))");

  }
  catch(const zypp::url::UrlException &e)
  {
    ZYPP_CAUGHT(e);
  }
}

BOOST_AUTO_TEST_CASE( test_url5)
{
  std::string str( "file:/some/${var:+path}/${var:-with}/${vars}" );
  BOOST_CHECK_EQUAL( Url(str).asString(), str );
  BOOST_CHECK_EQUAL( Url(zypp::url::encode( str, URL_SAFE_CHARS )).asString(), str );
}

BOOST_AUTO_TEST_CASE(plugin_scriptpath)
{
  // plugin script path must not be rewritten
  for ( const std::string t : { "script", "script/", "/script", "/script/", "./script", "./script/" } )
  {
    BOOST_CHECK_EQUAL( Url("plugin:"+t).getPathName(),	t );
  }

  { // more cosmetic issue, but the RepoVarReplacer should
    // not change the string representation (-> "plugin:/script")
    Url u( "plugin:script?opt=val" );
    RepoInfo i;
    i.setBaseUrl( u );
    BOOST_CHECK_EQUAL( u.asString(), i.url().asString() );
  }

}

BOOST_AUTO_TEST_CASE(plugin_querystring_args)
{
  // url querysting options without value must be possible
  // e.g. for plugin schema
  Url u( "plugin:script?loptv=lvalue&v=optv&lopt=&o" );
  url::ParamMap pm( u.getQueryStringMap() );
  BOOST_CHECK_EQUAL( pm.size(), 4 );
  BOOST_CHECK_EQUAL( pm["loptv"], "lvalue" );
  BOOST_CHECK_EQUAL( pm["v"], "optv" );
  BOOST_CHECK_EQUAL( pm["lopt"], "" );
  BOOST_CHECK_EQUAL( pm["o"], "" );
}

BOOST_AUTO_TEST_CASE(bsc1234304)
{
  // Removing or adding queryparams should not alter the encoding of other queryparams.
  // Old code decoded and re-encoded all params. "__token__=exp=%3D%26" was changed to
  // "__token__=exp==%26" by this. That's not wrong, but we'd like to preserve the original
  // params if possible.
  Url u { "https://dl.suse.com/?__token__=exp=%3D%261862049599~acl=/SUSE/*~hmac=968c8dc9&t%3Dt" };
  BOOST_CHECK_EQUAL( u.getQueryString(), "__token__=exp=%3D%261862049599~acl=/SUSE/*~hmac=968c8dc9&t%3Dt" );
  u.delQueryParam("t=t");
  BOOST_CHECK_EQUAL( u.getQueryString(), "__token__=exp=%3D%261862049599~acl=/SUSE/*~hmac=968c8dc9" );
  u.setQueryParam("t=t", "a" );
  BOOST_CHECK_EQUAL( u.getQueryString(), "__token__=exp=%3D%261862049599~acl=/SUSE/*~hmac=968c8dc9&t%3Dt=a" );
  u.setQueryParam("t=t", "b" );
  BOOST_CHECK_EQUAL( u.getQueryString(), "__token__=exp=%3D%261862049599~acl=/SUSE/*~hmac=968c8dc9&t%3Dt=b" );

  u.setQueryParam("local", "b" );
  BOOST_CHECK_EQUAL( u.getQueryString(), "__token__=exp=%3D%261862049599~acl=/SUSE/*~hmac=968c8dc9&local=b&t%3Dt=b" );
  u.delQueryParams( { "t=t", "local" } );
  BOOST_CHECK_EQUAL( u.getQueryString(), "__token__=exp=%3D%261862049599~acl=/SUSE/*~hmac=968c8dc9" );

  // queryparams without value should not have a trailing "="
  u.setQueryParam("empty", "" );
  BOOST_CHECK_EQUAL( u.getQueryString(), "__token__=exp=%3D%261862049599~acl=/SUSE/*~hmac=968c8dc9&empty" );

}

BOOST_AUTO_TEST_CASE(bsc1238315)
{
  // Fix lost leading double slash when appending path to URLs.
  // A leading "//" in ordinary URLs is quite meaningless but would be preserved.
  // Important however with FTP whether a leading "//" is encoded as "/%2f".
  Url u;
  auto appendPathName = []( const Url & url_r, const Pathname & path_r, Url::EEncoding eflag_r = url::E_DECODED ) {
    Url ret { url_r };
    ret.appendPathName( path_r, eflag_r );
    return ret.asString();
  };

  // No difference with singe "/" URLs:
  u = Url("http://HOST/fo%20o");
  BOOST_CHECK_EQUAL( appendPathName( u, "ba%20a", url::E_ENCODED ),    "http://host/fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "ba a" ),                      "http://host/fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "%2fba%20a", url::E_ENCODED ), "http://host/fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "/ba a" ),                     "http://host/fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "../ba%20a", url::E_ENCODED ), "http://host/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "../ba a" ),                   "http://host/ba%20a" );

  u = Url("ftp://HOST/fo%20o");
  BOOST_CHECK_EQUAL( appendPathName( u, "ba%20a", url::E_ENCODED ),    "ftp://host/fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "ba a" ),                      "ftp://host/fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "%2fba%20a", url::E_ENCODED ), "ftp://host/fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "/ba a" ),                     "ftp://host/fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "../ba%20a", url::E_ENCODED ), "ftp://host/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "../ba a" ),                   "ftp://host/ba%20a" );

  // FTP differs with double "//" URLs:
  u = Url("http://HOST//fo%20o");
  BOOST_CHECK_EQUAL( appendPathName( u, "ba%20a", url::E_ENCODED ),    "http://host//fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "ba a" ),                      "http://host//fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "%2fba%20a", url::E_ENCODED ), "http://host//fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "/ba a" ),                     "http://host//fo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "../ba%20a", url::E_ENCODED ), "http://host//ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "../ba a" ),                   "http://host//ba%20a" );

  u = Url("ftp://HOST//fo%20o");
  BOOST_CHECK_EQUAL( appendPathName( u, "ba%20a", url::E_ENCODED ),    "ftp://host/%2Ffo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "ba a" ),                      "ftp://host/%2Ffo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "%2fba%20a", url::E_ENCODED ), "ftp://host/%2Ffo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "/ba a" ),                     "ftp://host/%2Ffo%20o/ba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "../ba%20a", url::E_ENCODED ), "ftp://host/%2Fba%20a" );
  BOOST_CHECK_EQUAL( appendPathName( u, "../ba a" ),                   "ftp://host/%2Fba%20a" );
}

BOOST_AUTO_TEST_CASE(pathNameSetTrailingSlash)
{
  auto apply = []( const std::string & url_r, bool apply_r ) -> std::string {
    Url u { url_r };
    u.pathNameSetTrailingSlash( apply_r );
    return u.getPathName();
  };
  BOOST_CHECK_EQUAL( apply( "http://HOST", true ),  "" );
  BOOST_CHECK_EQUAL( apply( "http://HOST", false ), "" );
  BOOST_CHECK_EQUAL( apply( "http://HOST/", true ),  "/" );
  BOOST_CHECK_EQUAL( apply( "http://HOST/", false ), "/" );
  BOOST_CHECK_EQUAL( apply( "http://HOST//", true ),  "//" );
  BOOST_CHECK_EQUAL( apply( "http://HOST//", false ), "//" );
  BOOST_CHECK_EQUAL( apply( "http://HOST//foo", true ),  "//foo/" );
  BOOST_CHECK_EQUAL( apply( "http://HOST//foo", false ), "//foo" );
  BOOST_CHECK_EQUAL( apply( "http://HOST//foo/", true ),  "//foo/" );
  BOOST_CHECK_EQUAL( apply( "http://HOST//foo/", false ), "//foo" );
  BOOST_CHECK_EQUAL( apply( "http://HOST//foo//", true ),  "//foo//" );
  BOOST_CHECK_EQUAL( apply( "http://HOST//foo//", false ), "//foo" );
}

// vim: set ts=2 sts=2 sw=2 ai et:
