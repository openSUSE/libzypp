#include <tests/lib/TestSetup.h>
#include <zypp-core/parser/json.h>
#include <zypp-core/base/Exception.h>

using namespace zypp;

BOOST_AUTO_TEST_CASE(parse_escaped_utf8)
{
  {
    std::string data = "\"\\u0048\\u0065\\u006C\\u006C\\u006F\\u0020\\u0057\\u006F\\u0072\\u006C\\u0064\"";

    std::stringstream inStr( data, std::ios_base::in );
    json::Parser p;
    auto res = p.parse( zypp::InputStream(inStr) );
    BOOST_REQUIRE( res.is_valid() );
    BOOST_REQUIRE_EQUAL( res->asString(), std::string("\u0048\u0065\u006C\u006C\u006F\u0020\u0057\u006F\u0072\u006C\u0064") );
  }

  {
    std::string data  = "\"\\u041f\\u043e\\u043b\\u0442\\u043e\\u0440\\u0430 \\u0417\\u0435\\u043c\\u043b\\u0435\\u043a\\u043e\\u043f\\u0430\"";
    std::string bData = "\u041f\u043e\u043b\u0442\u043e\u0440\u0430 \u0417\u0435\u043c\u043b\u0435\u043a\u043e\u043f\u0430";

    std::stringstream inStr( data, std::ios_base::in );
    json::Parser p;
    auto res = p.parse( zypp::InputStream(inStr) );
    BOOST_REQUIRE( res.is_valid() );
    BOOST_REQUIRE_EQUAL( res->asString(), bData );
  }


  {
    // UTF-16 surrogate pair are used to encode values outside of the multilingual plane
    std::string data = "\"\\uD834\\uDD1E\"";
    std::string bData = "\U0001D11E";

    std::stringstream inStr( data, std::ios_base::in );
    json::Parser p;
    auto res = p.parse( zypp::InputStream(inStr) );
    BOOST_REQUIRE( res.is_valid() );
    BOOST_REQUIRE_EQUAL( std::string(res->asString()), bData );
  }

}

BOOST_AUTO_TEST_CASE(basic_parse)
{
  // some basic tests from https://github.com/nst/JSONTestSuite
  std::vector<std::string> jsonDocs {
    "[[]   ]",
    "[]",
    "[""]",
    "[\"a\"]",
    "[false]",
    "[null, 1, \"1\", {}]",
    "[null]",
    "[1\n]",
    " [1]",
    "[1,null,null,null,2]",
    "[2] ",
    "[0e+1]",
    "[0e1]",
    "[ 4]",
    "[-0.000000000000000000000000000000000000000000000000000000000000000000000000000001]",
    "",
    "[20e1]",
    "[123e65]",
    "[-0]",
    "[-123]",
    "[-1]",
    "[-0]",
    "[1E22]",
    "[1E-2]",
    "[1E+2]",
    "[123e45]",
    "[123.456e78]",
    "[1e-2]",
    "[1e+2]",
    "[123]",
    "[123.456789]",
    "{\"asd\":\"sdf\"}",
    "{\"a\":\"b\",\"a\":\"b\"}",
    "{\"a\":\"b\",\"a\":\"c\"}",
    "{}",
    "{\"\":0}",
    "{\"foo\\u0000bar\": 42}",
    "{ \"min\": -1.0e+28, \"max\": 1.0e+28 }",
    "{\"asd\":\"sdf\", \"dfg\":\"fgh\"}",
    "{\"x\":[{\"id\": \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}], \"id\": \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}",
    "{\"a\":[]}",
    "{\"title\":\"\\u041f\\u043e\\u043b\\u0442\\u043e\\u0440\\u0430 \\u0417\\u0435\\u043c\\u043b\\u0435\\u043a\\u043e\\u043f\\u0430\" }",
    "{\n\"a\": \"b\"\n}",
    "[\"\\\"\"]",
    "[\"\\\\a\"]",
    "[\"\\n\"]",
    "[\"\\u0012\"]",
    "[\"\\uFFFF\"]",
    "[\"asd\"]",
    "[ \"asd\"]",
    "[\"new\\u00A0line\"]",
    "[\"asd \"]",
    "\" \"",
    "[\n]",
    "false",
    "42",
    "-0.1",
    "null",
    "\"asd\"",
    "true",
    "\"\"",
    "[\"a\"]",
    "",
    "[true]",
    " [] ",
  };


  for ( unsigned int i = 0; i < jsonDocs.size(); i++ ) {
    std::stringstream inStr( jsonDocs[i], std::ios_base::in );
    json::Parser p;
    auto res = p.parse( zypp::InputStream(inStr) );
    if ( !res )
      std::cerr << " Error : " << res.error();
    BOOST_REQUIRE_MESSAGE( res.is_valid(), "Json Document index: " << i << " must parse." );
  }
}


BOOST_AUTO_TEST_CASE(basic_object)
{
  json::Object orig{
    {"key", "value"},
    {"key1", 2u },
    {"key2", -2 },
    {"key3", json::Array{1u,"str_in_arr",3u,4u, true, false, json::Null()} },
    {"multi", 1u },
    {"multi", json::Array{2u, json::Array{1u,2u,3u}, json::Object{ {"a", "b"} }} }
  };

#define OBJ_CHECK(o) \
  BOOST_REQUIRE_EQUAL( o.size(), 6 ); \
  BOOST_REQUIRE_EQUAL( o.contains("key"), true ); \
  BOOST_REQUIRE_EQUAL( o.contains("key1"), true ); \
  BOOST_REQUIRE_EQUAL( o.contains("key2"), true ); \
  BOOST_REQUIRE_EQUAL( o.contains("key3"), true ); \
  BOOST_REQUIRE_EQUAL( o.contains("multi"), true ); \
  BOOST_REQUIRE_EQUAL( o, o ); \
   \
  { \
    const auto &v1 = o.values("key"); \
    BOOST_REQUIRE_EQUAL(v1.size(), 1); \
    BOOST_REQUIRE_EQUAL(v1[0].type(), json::Value::StringType ); \
    BOOST_REQUIRE_EQUAL(v1[0].asString(), std::string("value") ); \
  } \
   \
  { \
    const auto &v1 = o.values("key1"); \
    BOOST_REQUIRE_EQUAL(v1.size(), 1); \
    BOOST_REQUIRE_EQUAL(v1[0].type(), json::Value::UIntType ); \
    BOOST_REQUIRE_EQUAL(v1[0].asUInt(), 2 ); \
  } \
 \
  { \
    const auto &v1 = o.values("key2"); \
    BOOST_REQUIRE_EQUAL(v1.size(), 1); \
    BOOST_REQUIRE_EQUAL(v1[0].type(), json::Value::IntType ); \
    BOOST_REQUIRE_EQUAL(v1[0].asInt(), -2 ); \
  } \
 \
  { \
    const auto &v1 = o.values("key3"); \
    BOOST_REQUIRE_EQUAL(v1.size(), 1); \
    BOOST_REQUIRE_EQUAL(v1[0].type(), json::Value::ArrayType ); \
 \
    const auto &arr = v1[0].asArray (); \
    BOOST_REQUIRE_EQUAL(arr.size(), 7 ); \
 \
    BOOST_REQUIRE_EQUAL(arr[0].type(), json::Value::UIntType ); \
    BOOST_REQUIRE_EQUAL(arr[0].asUInt(), 1 ); \
 \
    BOOST_REQUIRE_EQUAL(arr[1].type(), json::Value::StringType ); \
    BOOST_REQUIRE_EQUAL(arr[1].asString(), "str_in_arr" ); \
 \
    BOOST_REQUIRE_EQUAL(arr[2].type(), json::Value::UIntType ); \
    BOOST_REQUIRE_EQUAL(arr[2].asUInt(), 3 ); \
 \
    BOOST_REQUIRE_EQUAL(arr[3].type(), json::Value::UIntType ); \
    BOOST_REQUIRE_EQUAL(arr[3].asUInt(), 4 ); \
 \
    BOOST_REQUIRE_EQUAL(arr[4].type(), json::Value::BoolType ); \
    BOOST_REQUIRE_EQUAL(arr[4].asBool(), true ); \
 \
    BOOST_REQUIRE_EQUAL(arr[5].type(), json::Value::BoolType ); \
    BOOST_REQUIRE_EQUAL(arr[5].asBool(), false ); \
 \
    BOOST_REQUIRE_EQUAL(arr[6].type(), json::Value::NullType ); \
 \
  } \
 \
  const auto &multiVec = o.values("multi");\
  BOOST_REQUIRE_EQUAL( multiVec.size(), 2 );\
  BOOST_REQUIRE_EQUAL( multiVec[0].type(), json::Value::UIntType );\
  BOOST_REQUIRE_EQUAL( multiVec[0].asUInt(), 1 );\
\
  BOOST_REQUIRE_EQUAL( multiVec[1].type(), json::Value::ArrayType );\
  {\
    const auto &mulArray = multiVec[1].asArray();\
    BOOST_REQUIRE_EQUAL(mulArray.size(), 3);\
\
    BOOST_REQUIRE_EQUAL( mulArray[0].type(), json::Value::UIntType );\
    BOOST_REQUIRE_EQUAL( mulArray[0], 2u );\
\
    BOOST_REQUIRE_EQUAL( mulArray[1].type(), json::Value::ArrayType );\
    const auto &subArray1 = mulArray[1].asArray();\
    BOOST_REQUIRE_EQUAL( subArray1.size(), 3 );\
    BOOST_REQUIRE_EQUAL( subArray1[0], 1u );\
    BOOST_REQUIRE_EQUAL( subArray1[1], 2u );\
    BOOST_REQUIRE_EQUAL( subArray1[2], 3u );\
\
    BOOST_REQUIRE_EQUAL( mulArray[2].type(), json::Value::ObjectType );\
    const auto &subObject = mulArray[2].asObject();\
    BOOST_REQUIRE_EQUAL(subObject.size(), 1 );\
    BOOST_REQUIRE_EQUAL( subObject.contains("a"), true );\
\
    json::Value aVal = subObject.values ("a").front();\
    BOOST_REQUIRE_EQUAL( aVal, "b" );\
  }

  OBJ_CHECK(orig)

  constexpr std::string_view expected(
    "{\n"
    "\"key\": \"value\",\n"
    "\"key1\": 2,\n"
    "\"key2\": -2,\n"
    "\"key3\": [1, \"str_in_arr\", 3, 4, true, false, null],\n"
    "\"multi\": 1,\n"
    "\"multi\": [2, [1, 2, 3], {\n"
    "\"a\": \"b\"\n"
    "}]\n"
    "}"
  );

  const std::string &serialized = orig.asJSON();
  BOOST_REQUIRE_EQUAL( expected, serialized );

  {
    std::stringstream i( serialized, std::ios_base::in );
    json::Parser p;
    auto res = p.parse( zypp::InputStream(i) );
    BOOST_REQUIRE( res.is_valid());
    BOOST_REQUIRE_EQUAL( res->type(), json::Value::ObjectType );

    auto parsedObj = res->asObject ();
    OBJ_CHECK(parsedObj)
    BOOST_REQUIRE_EQUAL( orig, parsedObj );
  }

#undef OBJ_CHECK
}

BOOST_AUTO_TEST_CASE(parse_mirrorlist)
{
  InputStream inFile( TESTS_SRC_DIR "/parser/data/mirrorlist.json" );
  json::Parser p;
  auto parsed = p.parse (inFile);
  if ( !parsed )
    std::cout << parsed.error() << std::endl;

  BOOST_REQUIRE( parsed.is_valid () );
  BOOST_REQUIRE( parsed->type () == json::Value::ArrayType );

  const auto &topArray = parsed->asArray();
  for ( const json::Value &v: topArray ) {
    BOOST_REQUIRE( v.type() == json::Value::ObjectType );
    const auto &obj = v.asObject();

    /*
     Each obj contains:
    "mtime": 1745433180,
    "prio": 0,
    "time": "2025-04-23 18:33:00",
    "url": "https://ftp.uni-erlangen.de/opensuse/tumbleweed/repo/non-oss/"
    */

    BOOST_REQUIRE( obj.contains("mtime") );
    BOOST_REQUIRE( obj.contains("prio") );
    BOOST_REQUIRE( obj.contains("time") );
    BOOST_REQUIRE( obj.contains("url") );

    BOOST_REQUIRE_EQUAL( obj.value("mtime").type(), json::Value::UIntType );
    BOOST_REQUIRE_EQUAL( obj.value("prio").type(), json::Value::UIntType );
    BOOST_REQUIRE_EQUAL( obj.value("time").type(), json::Value::StringType );
    BOOST_REQUIRE_EQUAL( obj.value("url").type(), json::Value::StringType );
    BOOST_REQUIRE_NO_THROW( zypp::Url( obj.value("url").asString() ) );

  }
}

BOOST_AUTO_TEST_CASE(parse_escaped_url)
{
  const std::string in("[\"https:\\/\\/ftp.tu-chemnitz.de\\/pub\\/linux\\/opensuse\\/update\\/tumbleweed\\/\"]");
  std::stringstream inStr(in);

  json::Parser p;
  auto res = p.parse(inStr);

  BOOST_REQUIRE(res.is_valid ());
  BOOST_REQUIRE_EQUAL( res->type(), json::Value::ArrayType );

  const auto &arr = res->asArray ();
  BOOST_REQUIRE_EQUAL( arr[0].type(), json::Value::StringType );
  BOOST_REQUIRE_EQUAL( arr[0], "https://ftp.tu-chemnitz.de/pub/linux/opensuse/update/tumbleweed/" );
}




