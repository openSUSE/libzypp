#include <zypp/media/MediaManager.h>
#include <zypp/base/String.h>
#include <zypp/base/Logger.h>
#include <zypp/Pathname.h>

#include <string>
#include <list>
#include <iostream>
#include <cstdlib>

#include <signal.h>

#include "mymediaverifier.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test.hpp>

using boost::unit_test::test_suite;
using boost::unit_test::test_case;

using namespace zypp;
using namespace zypp::media;

bool       do_step = false;
int        do_quit = 0;

void quit(int)
{
    do_quit = 1;
}

void goon(int)
{
}

#define ONE_STEP(MSG) \
do { \
  DBG << "======================================" << std::endl; \
  DBG << "==>> " << MSG << std::endl; \
  DBG << "======================================" << std::endl; \
  if( do_step) { pause(); if( do_quit) exit(0); } \
} while(0);

BOOST_AUTO_TEST_CASE(strange_test)
{
  bool eject_src = false;
  bool close_src = false;
  {
      struct sigaction sa;
      sigemptyset(&sa.sa_mask);
      sa.sa_flags   = 0;
      sa.sa_handler = goon;
      sigaction(SIGINT,  &sa, NULL);
      sa.sa_handler = quit;
      sigaction(SIGTERM, &sa, NULL);

//       std::cerr << "ARGS=" << argc << std::endl;
//       for(int i=1; i < argc; i++)
//       {
//         if( std::string(argv[i]) == "-i")
//           do_step = true;
//         else
//         if( std::string(argv[i]) == "-e")
//           eject_src = true;
//         else
//         if( std::string(argv[i]) == "-c")
//           close_src = true;
//       }
  }

  MediaVerifierRef verifier(
    new MyMediaVerifier(/* "SUSE-Linux-CORE-i386 9" */)
  );
  MediaManager     mm;
  media::MediaId   src = 0;
  media::MediaId   iso;
  zypp::Url        src_url;
  zypp::Url        iso_url;

  src_url = "nfs://dist.suse.de/dist/install";

  iso_url = "iso:/";
  iso_url.setQueryParam("iso", "SUSE-10.1-Beta5/SUSE-Linux-10.1-beta5-i386-CD1.iso");
  iso_url.setQueryParam("url", src_url.asString());

/*
  iso_url = "iso:/";
  iso_url.setQueryParam("iso", "/space/tmp/iso/SUSE-Linux-10.1-beta7-i386-CD1.iso");
*/

  try
  {
    if( eject_src || close_src)
    {
      ONE_STEP("SRC: open " + src_url.asString());
      src = mm.open(src_url);

      ONE_STEP("SRC: attach")
      mm.attach(src);
    }

    ONE_STEP("ISO: open " + iso_url.asString());
    iso = mm.open(iso_url);

    ONE_STEP("ISO: add verifier")
    mm.addVerifier(iso, verifier);

    ONE_STEP("ISO: attach")
    mm.attach(iso);

    ONE_STEP("provideFile(/INDEX.gz)")
    mm.provideFile(iso, Pathname("/INDEX.gz"));

    if( eject_src)
    {
      try
      {
        ONE_STEP("SRC: release(ejectDev=\"/dev/device\")")
        mm.release(src);//! \todo add the device argument once mm.getDevices() is ready
      }
      catch(const MediaException &e)
      {
        ZYPP_CAUGHT(e);
        ERR << "ONE: HUH? Eject hasn't worked?!" << std::endl;
      }
    }
    else
    if( close_src)
    {
      try
      {
        ONE_STEP("SRC: close()")
        mm.close(src);
      }
      catch(const MediaException &e)
      {
        ZYPP_CAUGHT(e);
        ERR << "SRC: HUH? Close hasn't worked?!" << std::endl;
      }
    }

    ONE_STEP("ISO: RELEASE")
    mm.release(iso);

    ONE_STEP("CLEANUP")
  }
  catch(const MediaException &e)
  {
    ERR << "Catched media exception..." << std::endl;
    ZYPP_CAUGHT(e);
  }
  catch( ... )
  {
    // hmm...
    ERR << "Catched *unknown* exception" << std::endl;
  }
}

// vim: set ts=2 sts=2 sw=2 ai et:
