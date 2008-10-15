#include <stdio.h>
#include <iostream>
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/parameterized_test.hpp>
#include <boost/test/unit_test_log.hpp>

#include "zypp/MediaSetAccess.h"
#include "zypp/Url.h"

using std::cout;
using std::endl;
using std::string;
using namespace zypp;
using namespace boost::unit_test;


class SimpleVerifier : public media::MediaVerifierBase
{
public:

  SimpleVerifier( const std::string &id )
  {
    _media_id = id;
  }

  virtual bool isDesiredMedia(const media::MediaAccessRef &ref)
  {
    return ref->doesFileExist(Pathname("/." + _media_id ));
  }

private:
  std::string _media_id;
};

bool check_file_exists(const Pathname &path)
{
  FILE *file;

  if ((file = fopen(path.asString().c_str(), "r")) == NULL) return false;

  fclose(file);
  return true;
}

/*
 * Check how MediaSetAccess::rewriteUrl() works.
 */
BOOST_AUTO_TEST_CASE(msa_url_rewrite)
{
  BOOST_CHECK_EQUAL(
    MediaSetAccess::rewriteUrl(Url("iso:/?iso=/path/to/CD1.iso"), 1).asString(),
    Url("iso:/?iso=/path/to/CD1.iso").asString());

  BOOST_CHECK_EQUAL(
    MediaSetAccess::rewriteUrl(Url("iso:/?iso=/path/to/CD1.iso"), 2).asString(),
    Url("iso:/?iso=/path/to/CD2.iso").asString());

  BOOST_CHECK_EQUAL(
    MediaSetAccess::rewriteUrl(Url("iso:/?iso=/path/to/CD1.iso"), 13).asString(),
    Url("iso:/?iso=/path/to/CD13.iso").asString());

  BOOST_CHECK_EQUAL(
    MediaSetAccess::rewriteUrl(Url("iso:/?iso=/path/to/cd1.iso"), 2).asString(),
    Url("iso:/?iso=/path/to/cd2.iso").asString());

  BOOST_CHECK_EQUAL(
    MediaSetAccess::rewriteUrl(Url("iso:/?iso=/path/to/cd2.iso"), 1).asString(),
    Url("iso:/?iso=/path/to/cd1.iso").asString());

  BOOST_CHECK_EQUAL(
    MediaSetAccess::rewriteUrl(Url("iso:/?iso=/path/to/dvd1.iso"), 2).asString(),
    Url("iso:/?iso=/path/to/dvd2.iso").asString());

  BOOST_CHECK_EQUAL(
    MediaSetAccess::rewriteUrl(Url("dir:/path/to/CD1"), 2).asString(),
    Url("dir:/path/to/CD2").asString());

  // trailing slash check
  BOOST_CHECK_EQUAL(
    MediaSetAccess::rewriteUrl(Url("dir:/path/to/CD1/"), 2).asString(),
    Url("dir:/path/to/CD2/").asString());

  BOOST_CHECK_EQUAL(
    MediaSetAccess::rewriteUrl(Url("nfs://nfs-server/exported/path/to/dvd1"), 2).asString(),
    Url("nfs://nfs-server/exported/path/to/dvd2").asString());

  // single media check  shouldn't this fail somehow??
  BOOST_CHECK_EQUAL(
    MediaSetAccess::rewriteUrl(Url("http://ftp.opensuse.org/pub/opensuse/distribution/SL-OSS-factory/inst-source"), 2).asString(),
    Url("http://ftp.opensuse.org/pub/opensuse/distribution/SL-OSS-factory/inst-source").asString());
}

#define DATADIR (string("dir:") + string(TESTS_SRC_DIR) + string("/zypp/data/mediasetaccess"))

/*
 *
 * test data dir structure:
 *
 * .
 * |-- src1
 * |   |-- cd1
 * |   |   |-- dir
 * |   |   |   |-- file1
 * |   |   |   |-- file2
 * |   |   |   `-- subdir
 * |   |   |       `-- file
 * |   |   `-- test.txt
 * |   |-- cd2
 * |   |   `-- test.txt
 * |   `-- cd3
 * |       `-- test.txt
 * `-- src2
 *     `-- test.txt
 *
 */

/*
 * Provide files from set without verifiers.
 */
BOOST_AUTO_TEST_CASE(msa_provide_files_set)
{
  string urlstr = DATADIR + "/src1/cd1";
  Url url(urlstr);
  MediaSetAccess setaccess(url);

  Pathname file1 = setaccess.provideFile("/test.txt", 1);
  BOOST_CHECK(check_file_exists(file1) == true);

  Pathname file2 = setaccess.provideFile("/test.txt", 2);
  BOOST_CHECK(check_file_exists(file2) == true);

  Pathname file3 = setaccess.provideFile("/test.txt", 3);
  BOOST_CHECK(check_file_exists(file3) == true);
}

/*
 * Provide files from set with verifiers.
 */
BOOST_AUTO_TEST_CASE(msa_provide_files_set_verified)
{
  string urlstr = DATADIR + "/src1/cd1";
  Url url(urlstr);
  MediaSetAccess setaccess(url);

  setaccess.setVerifier(1, media::MediaVerifierRef(new SimpleVerifier("media1")));
  setaccess.setVerifier(2, media::MediaVerifierRef(new SimpleVerifier("media2")));
  setaccess.setVerifier(3, media::MediaVerifierRef(new SimpleVerifier("media3")));

  // provide file from media1
  Pathname file1 = setaccess.provideFile("/test.txt", 1);
  BOOST_CHECK(check_file_exists(file1) == true);

  // provide file from invalid media
  BOOST_CHECK_THROW(setaccess.provideFile("/test.txt", 2),
                    media::MediaNotDesiredException);

  // provide file from media3
  Pathname file3 = setaccess.provideFile("/test.txt", 3);
  BOOST_CHECK(check_file_exists(file3) == true);
}

/*
 * Provide file from single media with verifier.
 */
BOOST_AUTO_TEST_CASE(msa_provide_files_single)
{
  string urlstr = DATADIR + "/src2";
  Url url(urlstr);
  MediaSetAccess setaccess(url);
  setaccess.setVerifier(1, media::MediaVerifierRef(new SimpleVerifier("media")));

  // provide file from media
  Pathname file = setaccess.provideFile("/test.txt", 1);
  BOOST_CHECK(check_file_exists(file) == true);

  // provide non-existent file
  // (default answer from callback should be ABORT)
  BOOST_CHECK_THROW(setaccess.provideFile("/imnothere", 2),
                    media::MediaFileNotFoundException);
}

/*
 * Provide directory from src/cd1.
 */
BOOST_AUTO_TEST_CASE(msa_provide_dir)
{
  string urlstr = DATADIR + "/src1/cd1";
  Url url(urlstr);
  MediaSetAccess setaccess(url);

  Pathname dir = setaccess.provideDir("/dir", false, 1);

  Pathname file1 = dir + "/file1";
  BOOST_CHECK(check_file_exists(file1) == true);

  Pathname file2 = dir + "/file2";
  BOOST_CHECK(check_file_exists(file2) == true);

  // provide non-existent dir
  // (default answer from callback should be ABORT)
  BOOST_CHECK_THROW(setaccess.provideDir("/imnothere", 2),
                    media::MediaFileNotFoundException);

  // This can't be properly tested with 'dir' schema, probably only curl
  // schemas (http, ftp) where download is actually needed.
  // Other schemas just get mounted onto a local dir and the whole subtree
  // is automatically available that way.
  // BOOST_CHECK(check_file_exists(dir + "/subdir/file") == false);
  // BOOST_CHECK(check_file_exists(dir + "/subdir") == false);
}


/*
 * Provide directory from src/cd1 (recursively).
 */
BOOST_AUTO_TEST_CASE(msa_provide_dirtree)
{
  string urlstr = DATADIR + "/src1/cd1";
  Url url(urlstr);
  MediaSetAccess setaccess(url);

  Pathname dir = setaccess.provideDir("/dir", true, 1);

  Pathname file1 = dir + "/file1";
  BOOST_CHECK(check_file_exists(file1) == true);

  Pathname file2 = dir + "/file2";
  BOOST_CHECK(check_file_exists(file2) == true);

  Pathname file3 = dir + "/subdir/file";
  BOOST_CHECK(check_file_exists(file3) == true);
}

/*
 * Provide optional file
 */
BOOST_AUTO_TEST_CASE(msa_provide_optional_file)
{
  MediaSetAccess setaccess(Url(DATADIR + "/src1/cd1"));

  // must not throw
  BOOST_CHECK(setaccess.provideOptionalFile("/foo", 1).empty() == true);

  Pathname file = setaccess.provideOptionalFile("/test.txt", 1);
  BOOST_CHECK(check_file_exists(file) == true);
  
  //! \todo test provideOptionalFile with not desired media
}

// vim: set ts=2 sts=2 sw=2 ai et:
