#include <clocale>

#include <boost/test/unit_test.hpp>

#include <zypp-tui/output/Out.h>
#include <zypp-tui/utils/text.h>

using namespace ztui;

namespace
{
  struct Utf8Locale
  {
    Utf8Locale()
    {
      BOOST_REQUIRE_MESSAGE( std::setlocale( LC_CTYPE, "C.UTF-8" ),
                             "C.UTF-8 locale is required for this test" );
    }
  };
}

BOOST_FIXTURE_TEST_CASE( crush_counts_terminal_columns, Utf8Locale )
{
  TermLine line( TermLine::SplitFlags( TermLine::SF_CRUSH ) );
  line.lhs << "Installing: 软件包 ";
  line.rhs << "[100%]";

  const std::string output = line.get( 20 );

  BOOST_CHECK_EQUAL( mbs_width( output ), 20 );
  BOOST_CHECK_EQUAL( output, "Installing: 软[100%]" );
}

BOOST_FIXTURE_TEST_CASE( crush_does_not_split_wide_characters, Utf8Locale )
{
  TermLine line( TermLine::SplitFlags( TermLine::SF_CRUSH ) );
  line.rhs << "软件包";

  const std::string output = line.get( 5 );

  BOOST_CHECK_EQUAL( mbs_width( output ), 5 );
  BOOST_CHECK_EQUAL( output, "软件 " );
}

BOOST_FIXTURE_TEST_CASE( split_counts_terminal_columns, Utf8Locale )
{
  TermLine line( TermLine::SplitFlags( TermLine::SF_SPLIT ) );
  line.lhs << "state";
  line.rhs << "软件包";

  const std::string output = line.get( 5 );

  BOOST_CHECK_EQUAL( output, "state\n软件 " );
}
