#include <stdio.h>
#include <iostream>
#include <boost/test/unit_test.hpp>

#include <zypp-core/base/Logger.h>
#include <zypp-core/Url.h>
#include <zypp/PathInfo.h>
#include <zypp/TmpPath.h>

#include <zypp/ng/repo/downloader.h>
#include <zypp/ng/repo/workflows/susetags.h>
#include <zypp/ng/context.h>
#include <zypp-media/ng/providespec.h>

#include <zypp-common/KeyManager.h>
#include <zypp/zypp_detail/keyring_p.h>

#include "tests/zypp/KeyRingTestReceiver.h"


using std::cout;
using std::endl;
using std::string;
using namespace zypp;
using namespace boost::unit_test;
using namespace zypp::repo;

#define DATADIR (Pathname(TESTS_SRC_DIR) + "/repo/susetags/data")

using namespace zyppng::operators;

BOOST_AUTO_TEST_CASE(susetags_download)
{
  KeyRingTestReceiver keyring_callbacks;
  keyring_callbacks.answerAcceptKey(KeyRingReport::KEY_TRUST_TEMPORARILY);

  Pathname p = DATADIR + "/stable-x86-subset";
  RepoInfo repoinfo;
  repoinfo.setAlias("testrepo");
  repoinfo.setPath("/");

  filesystem::TmpDir tmp;
  Pathname localdir(tmp.path());


  auto ctx = zyppng::Context::create ();
  ctx->keyRing ()->allowPreload( false );

  auto res = ctx->provider()->attachMedia( p.asDirUrl() , zyppng::ProvideMediaSpec() )
  | and_then( [&]( zyppng::ProvideMediaHandle h ){
    auto dlctx = std::make_shared<zyppng::repo::DownloadContext>( ctx, repoinfo, localdir );
    return zyppng::SuseTagsWorkflows::download(dlctx, h);
  })
  | and_then( [&](zyppng::repo::DownloadContextRef ctx ) {

    MIL << "All files downloaded" << endl;

    const char* files[] =
    {
      "/suse",
      "/suse/setup",
      "/suse/setup/descr",
      "/suse/setup/descr/kde-10.3-71.noarch.pat",
      "/suse/setup/descr/packages",
      "/suse/setup/descr/packages.DU",
      "/suse/setup/descr/packages.en",
  //    "/suse/setup/descr/packages.es",
      "/suse/setup/descr/patterns",
      "/content",
      "/gpg-pubkey-7e2e3b05-44748aba.asc",
      "/media.1",
  //    "/media.1/products.asc",
  //    "/media.1/products.key",
      "/media.1/media",
  //    "/media.1/products",
  //    "/media.1/info.txt",
  //    "/media.1/license.zip",
      "/gpg-pubkey-a1912208-446a0899.asc",
      "/gpg-pubkey-307e3d54-44201d5d.asc",
      "/gpg-pubkey-9c800aca-40d8063e.asc",
      "/content.asc",
      "/content.key",
      "/gpg-pubkey-3d25d3d9-36e12d04.asc",
      "/gpg-pubkey-0dfb3188-41ed929b.asc",
      NULL
    };

    int i=0;
    while ( files[i] != NULL )
    {
      BOOST_CHECK_MESSAGE( PathInfo(localdir + files[i] ).isExist(), string(files[i]).c_str() );
      i++;
    }

    return zyppng::expected<void>::success();
  });

  BOOST_REQUIRE ( res.is_valid () );
}


BOOST_AUTO_TEST_CASE(susetags_gz_download)
{
  KeyRingTestReceiver keyring_callbacks;
  keyring_callbacks.answerAcceptKey(KeyRingReport::KEY_TRUST_TEMPORARILY);

  Pathname p = DATADIR + "/stable-x86-subset-gz";

  RepoInfo repoinfo;
  repoinfo.setAlias("testrepo");
  repoinfo.setPath("/");
  filesystem::TmpDir tmp;

  Pathname localdir(tmp.path());

  auto ctx = zyppng::Context::create ();
  ctx->keyRing ()->allowPreload( false );

  {
    // remove the key imported in the previous test, this should force the workflow to download the content.key file
    ctx->keyRing()->deleteKey( "3FB89E3A27C6B0E4" );
    BOOST_REQUIRE_MESSAGE( !ctx->keyRing()->isKeyTrusted( "3FB89E3A27C6B0E4" ), "Key should be gone from trusted!" );
    BOOST_REQUIRE_MESSAGE( !ctx->keyRing()->isKeyKnown  ( "3FB89E3A27C6B0E4" ), "Key should be gone from known!" );
  }

  auto res = ctx->provider()->attachMedia( p.asDirUrl() , zyppng::ProvideMediaSpec() )
  | and_then( [&]( zyppng::ProvideMediaHandle h ){
    auto dlctx = std::make_shared<zyppng::repo::DownloadContext>( ctx, repoinfo, localdir );
    return zyppng::SuseTagsWorkflows::download(dlctx, h);
  })
  | and_then( [&](zyppng::repo::DownloadContextRef ctx ) {

    const char* files[] =
    {
      "/suse",
      "/suse/setup",
      "/suse/setup/descr",
      "/suse/setup/descr/kde-10.3-71.noarch.pat.gz",
      "/suse/setup/descr/packages.gz",
      "/suse/setup/descr/packages.DU.gz",
      "/suse/setup/descr/packages.en.gz",
  //    "/suse/setup/descr/packages.es",
      "/suse/setup/descr/patterns.gz",
      "/content",
      "/gpg-pubkey-7e2e3b05-44748aba.asc",
      "/media.1",
  //    "/media.1/products.asc",
  //    "/media.1/products.key",
      "/media.1/media",
  //    "/media.1/products",
  //    "/media.1/info.txt",
  //    "/license.tar.gz",
  //    "/control.xml",
  //    "/installation.xml",
      "/gpg-pubkey-a1912208-446a0899.asc",
      "/gpg-pubkey-307e3d54-44201d5d.asc",
      "/gpg-pubkey-9c800aca-40d8063e.asc",
      "/content.asc",
      "/content.key",
      "/gpg-pubkey-3d25d3d9-36e12d04.asc",
      "/gpg-pubkey-0dfb3188-41ed929b.asc",
      NULL
    };

    int i=0;
    while ( files[i] != NULL )
    {
      BOOST_CHECK_MESSAGE( PathInfo(localdir + files[i] ).isExist(), string(files[i]).c_str() );
      i++;
    }

    return zyppng::expected<void>::success();

  });

  BOOST_REQUIRE ( res.is_valid () );
}

// vim: set ts=2 sts=2 sw=2 ai et:
