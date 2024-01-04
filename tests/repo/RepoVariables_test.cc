#include <boost/test/unit_test.hpp>
#include <iostream>
#include <list>
#include <map>

#include <zypp/ZConfig.h>
#include <zypp/Pathname.h>
#include <zypp/Url.h>
#include <zypp/base/ValueTransform.h>
#include <zypp/repo/RepoVariables.h>

using std::cout;
using std::endl;
using namespace zypp;
using namespace boost::unit_test;

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

#define DATADIR (Pathname(TESTS_SRC_DIR) +  "/repo/yum/data")

typedef std::list<std::string> ListType;

namespace std {
  std::ostream & operator<<( std::ostream & str, const ListType & obj )
  {
    str << "[";
    for ( const auto & el : obj )
      str << " " << el;
    return str << " ]";
  }
}

// A plain functor
struct FncTransformator
{
  std::string operator()( const std::string & value_r ) const
  { return "{"+value_r+"}"; }
};

BOOST_AUTO_TEST_CASE(value_transform)
{
  using zypp::base::ValueTransform;
  using zypp::base::ContainerTransform;

  typedef ValueTransform<std::string, FncTransformator> ReplacedString;
  typedef ContainerTransform<ListType, FncTransformator> ReplacedStringList;

  ReplacedString r( "val" );
  BOOST_CHECK_EQUAL( r.raw(), "val" );
  BOOST_CHECK_EQUAL( r.transformed(),	"{val}" );

  r.raw() = "new";
  BOOST_CHECK_EQUAL( r.raw(), "new" );
  BOOST_CHECK_EQUAL( r.transformed(),	"{new}" );

  ReplacedStringList rl;
  BOOST_CHECK_EQUAL( rl.empty(), true );
  BOOST_CHECK_EQUAL( rl.size(), 0 );
  BOOST_CHECK_EQUAL( rl.raw(), ListType() );
  BOOST_CHECK_EQUAL( rl.transformed(), ListType() );

  rl.raw().push_back("a");
  rl.raw().push_back("b");
  rl.raw().push_back("c");

  BOOST_CHECK_EQUAL( rl.empty(), false );
  BOOST_CHECK_EQUAL( rl.size(), 3 );
  BOOST_CHECK_EQUAL( rl.raw(), ListType({ "a","b","c" }) );
  BOOST_CHECK_EQUAL( rl.transformed(), ListType({ "{a}", "{b}", "{c}" }) );

  BOOST_CHECK_EQUAL( rl.transformed( rl.rawBegin() ), "{a}" );
}

void helperGenRepVarExpandResults()
{
  // Generate test result strings for RepVarExpand:
  //   ( STRING, REPLACED_all_vars_undef, REPLACED_all_vars_defined )
  // Carefully check whether new stings are correct before
  // adding them to the testccse.
  std::map<std::string,std::string> vartable;
  std::map<std::string,std::pair<std::string,std::string>> result;
  bool varsoff = true;

  auto varLookup = [&vartable,&varsoff]( const std::string & name_r )->const std::string *
  {
    if ( varsoff )
      return nullptr;
    std::string & val( vartable[name_r] );
    if ( val.empty() )
    { val = "["+name_r+"]"; }
    return &val;
  };

  for ( auto && value : {
    ""
    , "$"
    , "$${}"
    , "$_:"
    , "$_A:"
    , "$_A_:"
    , "$_A_B:"
    , "${_A_B}"
    , "\\${_A_B}"	// no escape on level 0
    , "${_A_B\\}"	// no close brace
    , "${C:-a$Bba}"
    , "${C:+a$Bba}"
    , "${C:+a${B}ba}"
    , "${C:+a\\$Bba}"	// escape on level > 0; no var $Bba
    , "${C:+a$Bba\\}"	// escape on level > 0; no close brace C
    , "${C:+a${B}ba}"
    , "${C:+a\\${B}ba}"	// escape on level > 0; no var ${B}
    , "${C:+a${B\\}ba}"	// escape on level > 0; no close brace B
    , "${C:+a\\${B\\}ba}"
    , "__${D:+\\$X--{${E:-==\\$X{o\\}==}  }--}__\\${B}${}__"
    , "__${D:+\\$X--{${E:-==\\$X{o\\}==}\\}--}__\\${B}${}__"
  } ) {
    varsoff = true;
    result[value].first = repo::RepoVarExpand()( value, varLookup );
    varsoff = false;
    result[value].second = repo::RepoVarExpand()( value, varLookup );
  }

  for ( const auto & el : result )
  {
#define CSTR(STR) str::form( "%-40s", str::gsub( "\""+STR+"\"", "\\", "\\\\" ).c_str() )
    cout << "RepVarExpandTest( " << CSTR(el.first) << ", " << CSTR(el.second.first) << ", " << CSTR(el.second.second) << " );" << endl;
  }
}

void RepVarExpandTest( const std::string & string_r, const std::string & allUndef_r, const std::string & allDef_r, bool hasVars=true )
{
  std::map<std::string,std::string> vartable;
  bool varsoff = true;

  auto varLookup = [&vartable,&varsoff]( const std::string & name_r )->const std::string *
  {
    if ( varsoff )
      return nullptr;
    std::string & val( vartable[name_r] );
    if ( val.empty() )
    { val = "["+name_r+"]"; }
    return &val;
  };

  BOOST_CHECK_EQUAL( repo::hasRepoVarsEmbedded( string_r ), hasVars );
  varsoff = true;
  BOOST_CHECK_EQUAL( repo::RepoVarExpand()( string_r, varLookup ), allUndef_r );
  varsoff = false;
  BOOST_CHECK_EQUAL( repo::RepoVarExpand()( string_r, varLookup ), allDef_r );
}

BOOST_AUTO_TEST_CASE(RepVarExpand)
{ //              ( STRING                                  , REPLACED_all_vars_undef                 , REPLACED_all_vars_defined                STRING has Vars)
  RepVarExpandTest( ""                                      , ""                                      , ""                                       , false );
  RepVarExpandTest( "$"                                     , "$"                                     , "$"                                      , false );
  RepVarExpandTest( "$${}"                                  , "$${}"                                  , "$${}"                                   , false );
  RepVarExpandTest( "$_:"                                   , "$_:"                                   , "[_]:"                                   );
  RepVarExpandTest( "$_A:"                                  , "$_A:"                                  , "[_A]:"                                  );
  RepVarExpandTest( "$_A_:"                                 , "$_A_:"                                 , "[_A_]:"                                 );
  RepVarExpandTest( "$_A_B:"                                , "$_A_B:"                                , "[_A_B]:"                                );
  RepVarExpandTest( "${C:+a$Bba\\}"                         , "${C:+a$Bba\\}"                         , "${C:+a[Bba]\\}"                         );
  RepVarExpandTest( "${C:+a$Bba}"                           , ""                                      , "a[Bba]"                                 );
  RepVarExpandTest( "${C:+a${B\\}ba}"                       , ""                                      , "a${B}ba"                                );
  RepVarExpandTest( "${C:+a${B}ba}"                         , ""                                      , "a[B]ba"                                 );
  RepVarExpandTest( "${C:+a\\$Bba}"                         , ""                                      , "a$Bba"                                  );
  RepVarExpandTest( "${C:+a\\${B\\}ba}"                     , ""                                      , "a${B}ba"                                );
  RepVarExpandTest( "${C:+a\\${B}ba}"                       , "ba}"                                   , "a${Bba}"                                );
  RepVarExpandTest( "${C:-a$Bba}"                           , "a$Bba"                                 , "[C]"                                    );
  RepVarExpandTest( "${_A_B\\}"                             , "${_A_B\\}"                             , "${_A_B\\}"                              , false );
  RepVarExpandTest( "${_A_B}"                               , "${_A_B}"                               , "[_A_B]"                                 );
  RepVarExpandTest( "\\${_A_B}"                             , "\\${_A_B}"                             , "\\[_A_B]"                               );
  RepVarExpandTest( "__${D:+\\$X--{${E:-==\\$X{o\\}==}  }--}__\\${B}${}__", "__--}__\\${B}${}__"      , "__$X--{[E]  --}__\\[B]${}__"            );
  RepVarExpandTest( "__${D:+\\$X--{${E:-==\\$X{o\\}==}\\}--}__\\${B}${}__", "____\\${B}${}__"         , "__$X--{[E]}--__\\[B]${}__"              );
}

void varInAuthExpect( const Url & url_r, const std::string & expHost_r, const std::string & expPort_r, const std::string & expPath_r,
                      const std::string & user_r = std::string(), const std::string & pass_r = std::string() )
{
  BOOST_CHECK_EQUAL( url_r.getHost(),     expHost_r );
  BOOST_CHECK_EQUAL( url_r.getPort(),     expPort_r );
  BOOST_CHECK_EQUAL( url_r.getPathName(), expPath_r );
  BOOST_CHECK_EQUAL( url_r.getUsername(), user_r );
  BOOST_CHECK_EQUAL( url_r.getPassword(), pass_r );
}

BOOST_AUTO_TEST_CASE(replace_text)
{
  /* check RepoVariablesStringReplacer */
  ZConfig::instance().setSystemArchitecture(Arch("i686"));
  ::setenv( "ZYPP_REPO_RELEASEVER", "13.2", 1 );

  repo::RepoVariablesStringReplacer replacer1;
  BOOST_CHECK_EQUAL( replacer1(""),		"" );
  BOOST_CHECK_EQUAL( replacer1("$"),		"$" );
  BOOST_CHECK_EQUAL( replacer1("$arc"),		"$arc" );
  BOOST_CHECK_EQUAL( replacer1("$arch"),	"i686" );

  BOOST_CHECK_EQUAL( replacer1("$archit"),	"$archit" );
  BOOST_CHECK_EQUAL( replacer1("${rc}it"),	"${rc}it" );
  BOOST_CHECK_EQUAL( replacer1("$arch_it"),	"$arch_it" );

  BOOST_CHECK_EQUAL( replacer1("$arch-it"),	"i686-it" );
  BOOST_CHECK_EQUAL( replacer1("$arch it"),	"i686 it" );
  BOOST_CHECK_EQUAL( replacer1("${arch}it"),	"i686it" );

  BOOST_CHECK_EQUAL( replacer1("${arch}it$archit $arch"),	"i686it$archit i686" );
  BOOST_CHECK_EQUAL( replacer1("X${arch}it$archit $arch-it"),	"Xi686it$archit i686-it" );

  BOOST_CHECK_EQUAL( replacer1("${releasever}"),	"13.2" );
  BOOST_CHECK_EQUAL( replacer1("${releasever_major}"),	"13" );
  BOOST_CHECK_EQUAL( replacer1("${releasever_minor}"),	"2" );

  BOOST_CHECK_EQUAL(replacer1("http://foo/$arch/bar"), "http://foo/i686/bar");

  /* check RepoVariablesUrlReplacer */
  repo::RepoVariablesUrlReplacer replacer2;

  // first of all url with {} must be accepted:
  BOOST_CHECK_NO_THROW( Url("ftp://site.org/${arch}/?arch=${arch}") );
  BOOST_CHECK_NO_THROW( Url("ftp://site.org/${arch:-noarch}/?arch=${arch:-noarch}") );
  BOOST_CHECK_NO_THROW( Url("ftp://site.org/${arch:+somearch}/?arch=${arch:+somearch}") );

  BOOST_CHECK_EQUAL(replacer2(Url("ftp://user:secret@site.org/$arch/")).asCompleteString(),
                    "ftp://user:secret@site.org/i686/");

  BOOST_CHECK_EQUAL(replacer2(Url("http://user:my$arch@site.org/$basearch/")).asCompleteString(),
                    "http://user:my$arch@site.org/i386/");

  BOOST_CHECK_EQUAL(replacer2(Url("http://site.org/update/?arch=$arch")).asCompleteString(),
                    "http://site.org/update/?arch=i686");

  BOOST_CHECK_EQUAL(replacer2(Url("http://site.org/update/$releasever/?arch=$arch")).asCompleteString(),
                    "http://site.org/update/13.2/?arch=i686");

  // - bsc#1067605: Allow VAR in Url authority
  // fake some host name via $arch
  varInAuthExpect( replacer2(Url("ftp://$arch/path")),      "i686",     "", "/path" );
  varInAuthExpect( replacer2(Url("ftp://$arch:1234/path")), "i686", "1234", "/path" );
  // don't expand in user/pass!
  varInAuthExpect( replacer2(Url("ftp://$arch:$arch@$arch:1234/path")),	"i686", "1234", "/path", "$arch", "$arch" );
  // No support for complex vars:
  // BOOST_CHECK_NO_THROW( Url("ftp://${arch:-nosite}/path") );
  // BOOST_CHECK_NO_THROW( Url("ftp://${arch:+somesite}/path") );
}

BOOST_AUTO_TEST_CASE(uncached)
{
  ::setenv( "ZYPP_REPO_RELEASEVER", "13.2", 1 );
  repo::RepoVariablesStringReplacer replacer1;
  BOOST_CHECK_EQUAL( replacer1("${releasever}"),	"13.2" );
  ::setenv( "ZYPP_REPO_RELEASEVER", "13.3", 1 );
  BOOST_CHECK_EQUAL( replacer1("${releasever}"),	"13.3" );
}

BOOST_AUTO_TEST_CASE(replace_rawurl)
{
  std::string s;

  s = "http://$host/repositories";  // lowercase, so it works for both (Url stores host component lowercased)
  {
    Url u { s };
    RawUrl r { s };
    // check replacing...
    repo::RepoVariablesUrlReplacer replacer;
    std::map<std::string,std::string> vardef { ::zyppintern::repoVariablesGet() };

    vardef["host"] = ""; // make sure it's not defined
    ::zyppintern::repoVariablesSwap( vardef );
    BOOST_CHECK_THROW( replacer( u ).asCompleteString(), zypp::url::UrlNotAllowedException ); // Url scheme requires a host component
    BOOST_CHECK_THROW( replacer( r ).asCompleteString(), zypp::url::UrlNotAllowedException ); // Url scheme requires a host component

    vardef["host"] = "cdn.opensuse.org";
    ::zyppintern::repoVariablesSwap( vardef );
    BOOST_CHECK_EQUAL( replacer( u ).asCompleteString(), "http://cdn.opensuse.org/repositories" );
    BOOST_CHECK_EQUAL( replacer( r ).asCompleteString(), "http://cdn.opensuse.org/repositories" );

    vardef["host"] = "cdn.opensuse.org/pathadded";
    ::zyppintern::repoVariablesSwap( vardef );
    BOOST_CHECK_EQUAL( replacer( u ).asCompleteString(), "http://cdn.opensuse.org/pathadded/repositories" );
    BOOST_CHECK_EQUAL( replacer( r ).asCompleteString(), "http://cdn.opensuse.org/pathadded/repositories" );

    vardef["host"] = "cdn.opensuse.org:1234/pathadded";
    ::zyppintern::repoVariablesSwap( vardef );
    BOOST_CHECK_EQUAL( replacer( u ).asCompleteString(), "http://cdn.opensuse.org:1234/pathadded/repositories" );
    BOOST_CHECK_EQUAL( replacer( r ).asCompleteString(), "http://cdn.opensuse.org:1234/pathadded/repositories" );

    vardef["host"] = "//something making the Url invalid//";
    ::zyppintern::repoVariablesSwap( vardef );
    BOOST_CHECK_THROW( replacer( u ).asCompleteString(), zypp::url::UrlNotAllowedException ); // Url scheme requires a host component
    BOOST_CHECK_THROW( replacer( r ).asCompleteString(), zypp::url::UrlNotAllowedException ); // Url scheme requires a host component
  }

  // If embedded vars do not form a valid Url, RawUrl must be used to carry them.
  // But one should make sure the expanded string later forms a valid Url.
  s = "${OPENSUSE_DISTURL:-http://cdn.opensuse.org/repositories/}leap/repo";
  BOOST_CHECK_THROW( Url{ s }, zypp::url::UrlBadComponentException );
  {
    RawUrl r { s };
    BOOST_CHECK_EQUAL( r.getScheme(), "zypp-rawurl" );
    BOOST_CHECK_EQUAL( r.getFragment(), s );
    // Make sure a RawUrl (as Url or String) is not re-evaluated when fed
    // back into a RawUrl. I.e. no zypp-rawurl as payload of a zypp-rawurl.
    BOOST_CHECK_EQUAL( r, RawUrl( r ) );
    BOOST_CHECK_EQUAL( r, RawUrl( r.asCompleteString() ) );
    // Of course you can always do Url("zypp-rawurl:#zypp-rawurl:#$VAR"),
    // but then you probably know what you are doing.
    BOOST_CHECK_EQUAL( RawUrl( "zypp-rawurl:#zypp-rawurl:%23$VAR" ).asCompleteString(), "zypp-rawurl:#zypp-rawurl:%23$VAR" );
    BOOST_CHECK_EQUAL( Url( "zypp-rawurl:#zypp-rawurl:%23$VAR" ).asCompleteString(),    "zypp-rawurl:#zypp-rawurl:%23$VAR" );

    // check replacing...
    repo::RepoVariablesUrlReplacer replacer;
    std::map<std::string,std::string> vardef { ::zyppintern::repoVariablesGet() };

    vardef["OPENSUSE_DISTURL"] = ""; // make sure it's not defined
    ::zyppintern::repoVariablesSwap( vardef );
    BOOST_CHECK_EQUAL( replacer( r ).asCompleteString(), "http://cdn.opensuse.org/repositories/leap/repo" );

    vardef["OPENSUSE_DISTURL"] = "https://mymirror.org/"; // custom value
    ::zyppintern::repoVariablesSwap( vardef );
    BOOST_CHECK_EQUAL( replacer( r ).asCompleteString(), "https://mymirror.org/leap/repo" );

    vardef["OPENSUSE_DISTURL"] = "//something making the Url invalid//";
    ::zyppintern::repoVariablesSwap( vardef );
    BOOST_CHECK_THROW( replacer( r ).asCompleteString(), zypp::url::UrlBadComponentException ); // Url scheme is a required component
  }
}
// vim: set ts=2 sts=2 sw=2 ai et:
