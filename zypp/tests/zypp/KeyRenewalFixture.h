#ifndef ZYPP_TESTS_KEYRENEWALFIXTURE_H
#define ZYPP_TESTS_KEYRENEWALFIXTURE_H

#include <boost/test/unit_test.hpp>

#include <zypp/PathInfo.h>
#include <zypp/Edition.h>
#include <zypp-common/PublicKey.h>

namespace keyrenewal
{
  using namespace zypp;

  /** Fixture data: three versions of one key and a signed repo (see data/KeyRenewal/readme.md). */
  inline Pathname dataDir()
  { return Pathname( TESTS_SRC_DIR ) / "zypp/data/KeyRenewal"; }

  /** The key as rpm names it: gpg-pubkey-<version>-<release>. */
  inline Edition editionOf( const PublicKey & key_r )
  { return Edition( key_r.gpgPubkeyVersion(), key_r.gpgPubkeyRelease() ); }

  /** Copy a fixture repo ("repo" or "repo-sub") to \a dest_r; \a keyFile_r becomes its repodata/repomd.xml.key. */
  inline void copyRepo( const Pathname & dest_r, const Pathname & keyFile_r, const char * repoName_r = "repo" )
  {
    BOOST_REQUIRE_EQUAL( filesystem::copy_dir_content( dataDir()/repoName_r, dest_r ), 0 );
    BOOST_REQUIRE_EQUAL( filesystem::copy( keyFile_r, dest_r/"repodata/repomd.xml.key" ), 0 );
  }
}

#endif
