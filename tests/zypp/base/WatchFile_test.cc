#include <iostream>
#include <fstream>

#include "TestSetup.h"
#include "zypp/TmpPath.h"
#include "zypp/base/WatchFile.h"

#define BOOST_TEST_MODULE WatchFile

using namespace std;
using namespace zypp;
using namespace zypp::filesystem;

BOOST_AUTO_TEST_CASE(WatchFile_basic)
{
  ofstream out;
  TmpFile tmpfile;

  out.open(tmpfile.path().c_str());
  out << "hello\n";
  out.flush();
  out.close();

  WatchFile watcher(tmpfile.path());

  BOOST_CHECK(!watcher.hasChanged());

  out.open(tmpfile.path().c_str());
  out << "changed contents\n";
  out.flush();
  out.close();

  BOOST_CHECK(watcher.hasChanged());

}

