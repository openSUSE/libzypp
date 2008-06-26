#include "Tools.h"
#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <set>

#include <zypp/base/Hash.h>
#include <zypp/base/LogControl.h>
#include <zypp/base/LogTools.h>
#include "zypp/base/Exception.h"
#include "zypp/base/InputStream.h"
#include "zypp/base/DefaultIntegral.h"
#include <zypp/base/Function.h>
#include <zypp/base/Iterator.h>
#include <zypp/Pathname.h>
#include <zypp/ResStatus.h>
#include <zypp/Edition.h>
#include <zypp/CheckSum.h>
#include <zypp/Date.h>
#include <zypp/Rel.h>

using namespace std;
using namespace zypp;

///////////////////////////////////////////////////////////////////

static const Pathname sysRoot( "/Local/ma/GPG" );

///////////////////////////////////////////////////////////////////

struct KeyRingSignalsReceive : public callback::ReceiveReport<KeyRingSignals>
{
  KeyRingSignalsReceive()
  {
    connect();
  }
  virtual void trustedKeyAdded( const KeyRing &/*keyring*/, const PublicKey &/*key*/ )
  {
    USR << endl;
  }
  virtual void trustedKeyRemoved( const KeyRing &/*keyring*/, const PublicKey &/*key*/ )
  {
    USR << endl;
  }
};

///////////////////////////////////////////////////////////////////

void ltrusted()
{
  list<PublicKey> trustedPublicKeys;
  {
    //zypp::base::LogControl::TmpLineWriter shutUp;
    KeyRing_Ptr p( getZYpp()->keyRing() );
    trustedPublicKeys = p->trustedPublicKeys();
  }
  USR << "Trusted Keys " << trustedPublicKeys.size() << endl;
  for_( it, trustedPublicKeys.begin(), trustedPublicKeys.end() )
  {
    USR << *it << endl;
    USR << "  Creat: " << (*it).created() << endl;
    USR << "  Expir: " << (*it).expires() << endl;
  }
}

///////////////////////////////////////////////////////////////////
/******************************************************************
**
**      FUNCTION NAME : main
**      FUNCTION TYPE : int
*/
int main( int argc, char * argv[] )
{
  INT << "===[START]==========================================" << endl;
  --argc;
  ++argv;
  KeyRingSignalsReceive r;

  Source_Ref a( createSource( "dir:///schnell/CD-ARCHIVE/11.0/GM/DVD/i386/DVD1", "fifi" ) );

  if ( 1 )
  {
    //zypp::base::LogControl::TmpLineWriter shutUp;
    getZYpp()->initTarget( sysRoot );
  }

  INT << "===[DONE]===========================================" << endl;
  zypp::base::LogControl::TmpLineWriter shutUp;
  return 0;
}

/******************************************************************
**
**      FUNCTION NAME : main
**      FUNCTION TYPE : int
*/
int main2( int argc, char * argv[] )
{
  INT << "===[START]==========================================" << endl;
  --argc;
  ++argv;
  KeyRingSignalsReceive r;


  KeyRing_Ptr p( getZYpp()->keyRing() );

  //PublicKey ka("/Local/ma/GPG/all-keys/build-9c800aca-40d8063e.asc");
  PublicKey kb("/Local/ma/GPG/all-keys/build-9c800aca-481f343a.asc");

  SEC << "============================================" << endl;
  ltrusted();
  //p->importKey( ka, true );
  //ltrusted();
  p->importKey( kb, true );
  ltrusted();

  SEC << "============================================" << endl;

  if ( 1 )
  {
    zypp::base::LogControl::TmpLineWriter shutUp;
    getZYpp()->initTarget( sysRoot );
  }



  INT << "===[DONE]===========================================" << endl;
  zypp::base::LogControl::TmpLineWriter shutUp;
  return 0;
}

