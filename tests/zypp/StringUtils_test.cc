//
// tests for String utilities zypp/base/String.h
//
#include <iostream>
#include "zypp/base/LogTools.h"
#include "zypp/base/String.h"

#include <boost/test/auto_unit_test.hpp>

using boost::unit_test::test_case;

using namespace std;
using namespace zypp;

#undef DBG
#define DBG cout

void chk_splitFields( const std::string & line, unsigned num )
{
  std::vector<std::string> result;
  DBG << ">>>" << line << "<<<" << endl;
  BOOST_CHECK_EQUAL( str::splitFields( line,    std::back_inserter(result) ), num );
  DBG << result << endl;

}

BOOST_AUTO_TEST_CASE(edition)
{
  chk_splitFields( "",    0 );
  chk_splitFields( ":",   2 );
  chk_splitFields( "a",   1 );
  chk_splitFields( "a:",  2 );
  chk_splitFields( ":a",  2 );
  chk_splitFields( ":a:", 3 );
  chk_splitFields( "a:a", 2 );
}
