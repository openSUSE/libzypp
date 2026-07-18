#include <clocale>

#include <boost/test/unit_test.hpp>

#include <zypp-tui/output/Out.h>
#include <zypp-tui/output/Utf8.h>
#include <zypp-tui/utils/text.h>

using namespace ztui;

namespace
{
  struct Utf8Locale
  {
    std::string previous = std::setlocale( LC_CTYPE, nullptr );

    Utf8Locale()
    {
      BOOST_REQUIRE_MESSAGE( std::setlocale( LC_CTYPE, "C.UTF-8" ),
                             "C.UTF-8 locale is required for this test" );
    }

    ~Utf8Locale()
    { std::setlocale( LC_CTYPE, previous.c_str() ); }
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

BOOST_FIXTURE_TEST_CASE( utf8_counts_columns_and_ignores_sgr, Utf8Locale )
{
  BOOST_CHECK_EQUAL( utf8::string( "A软件包B" ).size(), 8 );
  BOOST_CHECK_EQUAL( utf8::string( "e\u0301" ).size(), 1 );
  BOOST_CHECK_EQUAL( utf8::string( "\033[1m\033[31mA软\033[32m件B\033[0m" ).length(), 6 );
  BOOST_CHECK_EQUAL( utf8::string( "\033[31m\033[0m" ).size(), 0 );
  BOOST_CHECK_EQUAL( utf8::string().size(), 0 );
}

BOOST_FIXTURE_TEST_CASE( utf8_substr_pads_clipped_columns, Utf8Locale )
{
  const utf8::string text( "A软件B" );
  BOOST_CHECK_EQUAL( text.substr( 0, 2 ).str(), "A " );
  BOOST_CHECK_EQUAL( text.substr( 2, 3 ).str(), " 件" );
  BOOST_CHECK_EQUAL( text.substr( 2, 2 ).str(), "  " );
  BOOST_CHECK_EQUAL( text.substr( 1, 4 ).str(), "软件" );
  BOOST_CHECK_EQUAL( text.substr( 3 ).str(), "件B" );
  BOOST_CHECK_EQUAL( text.substr().str(), text.str() );
}

BOOST_FIXTURE_TEST_CASE( utf8_substr_preserves_sgr_byte_spans, Utf8Locale )
{
  const utf8::string text( "\033[1m\033[31m软\033[32m件\033[0mB" );
  BOOST_CHECK_EQUAL( text.substr().str(), text.str() );
  BOOST_CHECK_EQUAL( text.substr( 0, 2 ).str(), "\033[1m\033[31m软\033[32m" );
  BOOST_CHECK_EQUAL( text.substr( 2, 2 ).str(), "件\033[0m" );
  BOOST_CHECK_EQUAL( text.substr( 0, 1 ).str(), "\033[1m\033[31m \033[32m" );
  BOOST_CHECK_EQUAL( text.substr( 1, 3 ).str(), " \033[32m件\033[0m" );
  BOOST_CHECK_EQUAL( utf8::string( "\033[31m软\033[0m" ).substr( 0, 2 ).str(),
                     "\033[31m软\033[0m" );
  BOOST_CHECK_EQUAL( utf8::string( "\033[31m\033[0m" ).substr().str(),
                     "\033[31m\033[0m" );
  BOOST_CHECK_EQUAL( utf8::string( "e\u0301软" ).substr( 0, 2 ).str(), "e\u0301 " );
}

BOOST_FIXTURE_TEST_CASE( utf8_substr_handles_empty_and_unbounded_ranges, Utf8Locale )
{
  const utf8::string text( "\033[31m软件\033[0m" );
  BOOST_CHECK_EQUAL( text.substr( 0, 0 ).str(), "" );
  BOOST_CHECK_EQUAL( text.substr( 4 ).str(), "" );
  BOOST_CHECK_EQUAL( text.substr( 5 ).str(), "" );
  BOOST_CHECK_EQUAL( text.substr( utf8::string::npos ).str(), "" );
  BOOST_CHECK_EQUAL( text.substr( 2, utf8::string::npos-1 ).str(), "件\033[0m" );
  BOOST_CHECK_EQUAL( utf8::string().substr().str(), "" );
}

BOOST_FIXTURE_TEST_CASE( crush_preserves_colored_cjk_text, Utf8Locale )
{
  TermLine line( TermLine::SplitFlags( TermLine::SF_CRUSH ) );
  line.lhs << "\033[31m软件\033[0m";
  line.rhs << "[ok]";
  const std::string output = line.get( 7 );
  BOOST_CHECK_EQUAL( output, "\033[31m软 \033[0m[ok]" );
  BOOST_CHECK_EQUAL( mbs_width( output ), 7 );

  TermLine rhs( TermLine::SplitFlags( TermLine::SF_CRUSH ) );
  rhs.rhs << "\033[31m软件包\033[0m";
  BOOST_CHECK_EQUAL( rhs.get( 5 ), "\033[31m软件 \033[0m" );
  BOOST_CHECK_EQUAL( mbs_width( rhs.get( 5 ) ), 5 );
}

BOOST_FIXTURE_TEST_CASE( split_preserves_colored_cjk_text, Utf8Locale )
{
  TermLine line( TermLine::SplitFlags( TermLine::SF_SPLIT ) );
  line.lhs << "state";
  line.rhs << "\033[31m软件包\033[0m";
  const std::string output = line.get( 5 );
  BOOST_CHECK_EQUAL( output, "state\n\033[31m软件 \033[0m" );
  BOOST_CHECK_EQUAL( mbs_width( output.substr( output.find( '\n' )+1 ) ), 5 );
}
