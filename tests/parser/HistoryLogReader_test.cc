#include "TestSetup.h"
#include <zypp/parser/HistoryLogReader.h>
#include <zypp/parser/ParseException.h>

using namespace zypp;

BOOST_AUTO_TEST_CASE(basic)
{
  std::vector<HistoryLogData::Ptr> history;
  parser::HistoryLogReader parser( TESTS_SRC_DIR "/parser/HistoryLogReader_test.dat",
				   parser::HistoryLogReader::Options(),
    [&history]( HistoryLogData::Ptr ptr )->bool {
      history.push_back( ptr );
      return true;
    } );

  BOOST_CHECK_EQUAL( parser.ignoreInvalidItems(), false );
  BOOST_CHECK_THROW( parser.readAll(), parser::ParseException );

  parser.setIgnoreInvalidItems( true );
  BOOST_CHECK_EQUAL( parser.ignoreInvalidItems(), true );

  history.clear();
  parser.readAll();

  BOOST_CHECK_EQUAL( history.size(), 9 );
  BOOST_CHECK( dynamic_pointer_cast<HistoryLogDataRepoAdd>	( history[0] ) );
  BOOST_CHECK( dynamic_pointer_cast<HistoryLogDataInstall>	( history[1] ) );
  BOOST_CHECK( dynamic_pointer_cast<HistoryLogDataInstall>	( history[2] ) );
  BOOST_CHECK( dynamic_pointer_cast<HistoryLogDataRemove>	( history[3] ) );
  BOOST_CHECK( dynamic_pointer_cast<HistoryLogDataRepoRemove>	( history[4] ) );
  BOOST_CHECK( dynamic_pointer_cast<HistoryLogDataRemove>	( history[5] ) );
  BOOST_CHECK( dynamic_pointer_cast<HistoryLogData>		( history[6] ) );
  BOOST_CHECK( dynamic_pointer_cast<HistoryLogDataStampCommand>	( history[7] ) );
  BOOST_CHECK( dynamic_pointer_cast<HistoryLogPatchStateChange>	( history[8] ) );

  BOOST_CHECK_EQUAL( (*history[1])[HistoryLogDataInstall::USERDATA_INDEX], "trans|ID" ); // properly (un)escaped?
  HistoryLogDataInstall::Ptr p = dynamic_pointer_cast<HistoryLogDataInstall>( history[1] );
  BOOST_CHECK_EQUAL( p->userdata(), "trans|ID" ); // properly (un)escaped?
}
