#include <iostream>
#include <fstream>

#include "TestSetup.h"
#include "zypp/TmpPath.h"
#include "zypp/base/WatchPath.h"

#define BOOST_TEST_MODULE WatchPath

using namespace std;
using namespace zypp;
using namespace zypp::filesystem;



BOOST_AUTO_TEST_CASE(WatchPath_basic)
{
  ofstream out;
  TmpFile tmpfile;

  out.open(tmpfile.path().c_str());
  out << "hello\n";
  out.flush();
  out.close();

  WatchPath watcher(tmpfile.path());

  BOOST_CHECK(!watcher.hasChanged());

  out.open(tmpfile.path().c_str());
  out << "changed contents\n";
  out.flush();
  out.close();

  BOOST_CHECK(watcher.hasChanged());

}

BOOST_AUTO_TEST_CASE(WatchPath_directory)
{
  ofstream out;
  TmpDir tmpdir;

  mkdir(tmpdir.path() + "subdir");

  // start with one file in each level

  out.open((tmpdir.path() + "1.txt").c_str());
  out << "hello\n";
  out.flush();
  out.close();

  out.open((tmpdir.path() + "subdir" + "sub1.txt").c_str());
  out << "hello\n";
  out.flush();
  out.close();

  WatchPath watcher(tmpdir.path());

  // We haven't done anything yet, so no changes

  BOOST_CHECK(!watcher.hasChanged());

  MIL << "Create new file 2.txt" << endl;

  out.open((tmpdir.path() + "2.txt").c_str());
  out << "new file\n";
  out.flush();
  out.close();

  BOOST_CHECK(watcher.hasChanged());

  // a subsequent call should shot nothing changed

  BOOST_CHECK(!watcher.hasChanged());

  unlink(tmpdir.path() + "subdir" + "sub1.txt");

  BOOST_CHECK(watcher.hasChanged());
  BOOST_CHECK(!watcher.hasChanged());

  // this is tricky, add a file, then modify it
  // the watcher will catch the addition because
  // the contained directory changed, but to catch
  // a modification later it would need to add a watch
  // to the added file

  out.open((tmpdir.path() + "subdir" + "sub2.txt").c_str());
  out << "hello\n";
  out.flush();
  out.close();

  BOOST_CHECK(watcher.hasChanged());
  BOOST_CHECK(!watcher.hasChanged());

  out.open((tmpdir.path() + "subdir" + "sub2.txt").c_str());
  out << "if it was not added to the watch list you wont see this\n";
  out.flush();
  out.close();

  BOOST_CHECK(watcher.hasChanged());
  BOOST_CHECK(!watcher.hasChanged());

}

