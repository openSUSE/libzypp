
#include <iostream>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include <zypp-core/base/Logger.h>
#include <zypp-core/base/Exception.h>
#include <zypp/KeyRing.h>
#include <zypp-common/PublicKey.h>
#include <zypp/TmpPath.h>
#include <zypp/target/private/keyrefresh_p.h>

#include <zypp/ng/context.h>
#include <zypp/ng/workflows/keyringwf.h>

#include <boost/test/unit_test.hpp>

#include "KeyRingTestReceiver.h"

using boost::unit_test::test_suite;
using boost::unit_test::test_case;

using namespace zypp;
using namespace zypp::filesystem;

#define DATADIR (Pathname(TESTS_SRC_DIR) +  "/zypp/data/KeyRing")

bool verifyFileSignatureWorkflow( KeyRing_Ptr ring, const Pathname &file, const std::string &filedesc, const Pathname &signature, bool & sigValid_r, const KeyContext &keycontext = KeyContext())
{
  sigValid_r = false;	// just in case....
  keyring::VerifyFileContext context( file, signature );
  context.shortFile( filedesc );
  context.keyContext( keycontext );
  const auto &res = zyppng::KeyRingWorkflow::verifyFileSignature( zyppng::Context::create(), ring, std::move(context) );
  sigValid_r = res.second.fileValidated();
  return res.second.fileAccepted();
}

bool verifyFileSignatureWorkflow( KeyRing_Ptr ring, const Pathname &file, const std::string filedesc, const Pathname &signature, const KeyContext &keycontext = KeyContext())
{
  keyring::VerifyFileContext context( file, signature );
  context.shortFile( filedesc );
  context.keyContext( keycontext );
  const auto &res = zyppng::KeyRingWorkflow::verifyFileSignature( zyppng::Context::create(), ring, std::move(context) );
  return res.second.fileAccepted();
}


BOOST_AUTO_TEST_CASE(keyring_test)
{
  PublicKey key( Pathname(DATADIR) + "public.asc" );

 /**
  * scenario #1
  * import a not trusted key
  * ask for accept, answer yes 'temporarily'
  */
  {
    KeyRingTestReceiver keyring_callbacks;
    KeyRingTestSignalReceiver receiver;
    // base sandbox for playing
    TmpDir tmp_dir;
    KeyRing_Ptr keyring = new KeyRing( tmp_dir.path() );

    BOOST_CHECK_EQUAL( keyring->publicKeys().size(), (unsigned) 0 );
    BOOST_CHECK_EQUAL( keyring->trustedPublicKeys().size(), (unsigned) 0 );

    keyring->importKey( key, false );

    BOOST_CHECK_EQUAL( keyring->publicKeys().size(), (unsigned) 1 );
    BOOST_CHECK_EQUAL( keyring->trustedPublicKeys().size(), (unsigned) 0 );

    BOOST_CHECK_MESSAGE( keyring->isKeyKnown( key.id() ), "Imported untrusted key should be known");
    BOOST_CHECK_MESSAGE( ! keyring->isKeyTrusted( key.id() ), "Imported untrusted key should be untrusted");

    keyring_callbacks.answerAcceptKey(KeyRingReport::KEY_TRUST_TEMPORARILY);
    bool to_continue = verifyFileSignatureWorkflow( keyring, DATADIR + "repomd.xml", "Blah Blah", DATADIR + "repomd.xml.asc");

    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptUnknownKey(), "Should not ask for unknown key, it was known");
    BOOST_CHECK_MESSAGE( keyring_callbacks.askedAcceptKey(), "Verify Signature Workflow with only 1 untrusted key should ask user wether to trust and/or import");
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptVerFailed(), "The signature validates");
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptUnsignedFile(), "It is a signed file, so dont ask the opposite");

    BOOST_CHECK_MESSAGE( to_continue, "We did not import, but we trusted and signature validates.");
  }

  /**
  * scenario #1.1
  * import a not trusted key
  * ask to accept, answer yes 'temporarily'
  * vorrupt the file and check
  */
  {
    KeyRingTestReceiver keyring_callbacks;
    KeyRingTestSignalReceiver receiver;
    // base sandbox for playing
    TmpDir tmp_dir;
    KeyRing_Ptr keyring = new KeyRing ( tmp_dir.path() );

    BOOST_CHECK_EQUAL( keyring->publicKeys().size(), (unsigned) 0 );
    BOOST_CHECK_EQUAL( keyring->trustedPublicKeys().size(), (unsigned) 0 );

    keyring->importKey( key, false );

    keyring_callbacks.answerAcceptKey(KeyRingReport::KEY_TRUST_TEMPORARILY);

    // now we will recheck with a corrupted file
    bool to_continue = verifyFileSignatureWorkflow( keyring, DATADIR + "repomd.xml.corrupted", "Blah Blah", DATADIR + "repomd.xml.asc");

    // check wether the user got the right questions
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptUnknownKey(), "Should not ask for unknown key, it was known");
    BOOST_CHECK_MESSAGE( keyring_callbacks.askedAcceptKey(), "Verify Signature Workflow with only 1 untrusted key should ask user wether to trust and/or import");
    BOOST_CHECK_MESSAGE( keyring_callbacks.askedAcceptVerFailed(), "The signature does not validates");
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptUnsignedFile(), "It is a signed file, so dont ask the opposite");

    BOOST_CHECK_MESSAGE( ! to_continue, "We did not continue with a corrupted file");
  }

   /**
  * scenario #1.2
  * import a not trusted key
  * ask for trust, answer yes
  * ask for import, answer no
  * check without signature
  */
  {
    KeyRingTestReceiver keyring_callbacks;
    KeyRingTestSignalReceiver receiver;
    // base sandbox for playing
    TmpDir tmp_dir;
    KeyRing_Ptr keyring = new KeyRing ( tmp_dir.path() );

    keyring->importKey( key, false );

    keyring_callbacks.answerAcceptKey(KeyRingReport::KEY_TRUST_TEMPORARILY);
    // now we will recheck with a unsigned file
    bool to_continue = verifyFileSignatureWorkflow( keyring, DATADIR + "repomd.xml", "Blah Blah", Pathname() );

    // check wether the user got the right questions
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptUnknownKey(), "Should not ask for unknown key, it was known");
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptKey(), "No signature, no key to trust");
    BOOST_CHECK_MESSAGE( keyring_callbacks.askedAcceptUnsignedFile(), "Ask the user wether to accept an unsigned file");
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptVerFailed(), "There is no signature to verify");

    BOOST_CHECK_MESSAGE( ! to_continue, "We did not continue with a unsigned file");
  }

 /** scenario #2
  * empty keyring
  * should ask for unknown key
  * answer no
  */
  {
    KeyRingTestReceiver keyring_callbacks;
    KeyRingTestSignalReceiver receiver;
    // base sandbox for playing
    TmpDir tmp_dir;
    KeyRing_Ptr keyring = new KeyRing( tmp_dir.path() );

    BOOST_CHECK_MESSAGE( ! keyring->isKeyKnown( key.id() ), "empty keyring has not known keys");

    //keyring_callbacks.answerAcceptUnknownKey(true);
    bool to_continue = verifyFileSignatureWorkflow( keyring, DATADIR + "repomd.xml", "Blah Blah", DATADIR + "repomd.xml.asc");
    BOOST_CHECK_MESSAGE(keyring_callbacks.askedAcceptUnknownKey(), "Should ask to accept unknown key, empty keyring");
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptKey(), "Unknown key cant be trusted");
    BOOST_CHECK_MESSAGE( ! to_continue, "We answered no to accept unknown key");
  }

  /** scenario #3
  * import trusted key
  * should ask nothing
  * should emit signal
  */
  {
    KeyRingTestReceiver keyring_callbacks;
    KeyRingTestSignalReceiver receiver;
    // base sandbox for playing
    TmpDir tmp_dir;
    KeyRing_Ptr keyring = new KeyRing( tmp_dir.path() );

    BOOST_CHECK_EQUAL( keyring->publicKeys().size(), (unsigned) 0 );
    BOOST_CHECK_EQUAL( keyring->trustedPublicKeys().size(), (unsigned) 0 );

    keyring->importKey( key, true );

    BOOST_CHECK_EQUAL( receiver._trusted_key_added_called, true );

    BOOST_CHECK_EQUAL( keyring->publicKeys().size(), (unsigned) 0 );
    BOOST_CHECK_EQUAL( keyring->trustedPublicKeys().size(), (unsigned) 1 );

    BOOST_CHECK_MESSAGE( keyring->isKeyKnown( key.id() ), "Imported trusted key should be known");
    BOOST_CHECK_MESSAGE( keyring->isKeyTrusted( key.id() ), "Imported trusted key should be trusted");

    bool to_continue = verifyFileSignatureWorkflow( keyring, DATADIR + "repomd.xml", "Blah Blah", DATADIR + "repomd.xml.asc");

    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptUnknownKey(), "Should not ask for unknown key, it was known");
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptKey(), "Verify Signature Workflow with only 1 untrusted key should ask user wether to trust and/or import");
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptVerFailed(), "The signature validates");
    BOOST_CHECK_MESSAGE( ! keyring_callbacks.askedAcceptUnsignedFile(), "It is a signed file, so dont ask the opposite");

    BOOST_CHECK_MESSAGE( to_continue, "We did not import, but we trusted and signature validates.");
  }
  //keyring.importKey( key, true );
  //BOOST_CHECK_EQUAL( receiver._trusted_key_added_called, true );
  //BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), 1 );

  /* check signature id can be extracted */

}

BOOST_AUTO_TEST_CASE(signature_test)
{
  PublicKey key( DATADIR + "public.asc" );

  {
    KeyRingTestReceiver keyring_callbacks;
    KeyRingTestSignalReceiver receiver;
    // base sandbox for playing
    TmpDir tmp_dir;
    KeyRing keyring( tmp_dir.path() );

    BOOST_CHECK_EQUAL( keyring.readSignatureKeyId( DATADIR + "repomd.xml.asc" ), "BD61D89BD98821BE" );
    BOOST_CHECK_THROW( keyring.readSignatureKeyId(Pathname()), Exception );
    TmpFile tmp;
    BOOST_CHECK_EQUAL( keyring.readSignatureKeyId(tmp.path()), "" );

    keyring.importKey(key);

    BOOST_CHECK(keyring.verifyFileSignature( DATADIR + "repomd.xml", DATADIR + "repomd.xml.asc"));
    BOOST_CHECK( ! keyring.verifyFileSignature( DATADIR + "repomd.xml.corrupted", DATADIR + "repomd.xml.asc"));
  }
}

BOOST_AUTO_TEST_CASE(keyring_import)
{
  // base sandbox for playing
  TmpDir tmp_dir;
  KeyRing keyring( tmp_dir.path() );
  struct Receiver: public callback::ReceiveReport<KeyRingSignals>
  {
    Receiver()
    { connect(); }

    virtual void trustedKeyAdded( const PublicKey & key_r )
    { ++_cbcnt; }

    virtual void trustedKeyRemoved( const PublicKey & key_r )
    { --_cbcnt; }

    unsigned _cbcnt = 0;
  } receiver;

  ///////////////////////////////////////////////////////////////////
  // Make sure we get a proper callback notification if multiple
  // keys are imported at once.
  ///////////////////////////////////////////////////////////////////
  PublicKey key( DATADIR + "installkey.gpg" );
  BOOST_CHECK_EQUAL( key.hiddenKeys().size(), 2 );
  BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), 0 );
  keyring.importKey( key, true );
  BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), 3 );
  BOOST_CHECK_EQUAL( receiver._cbcnt, keyring.trustedPublicKeys().size() );
}

namespace
{
  /** Records the key ring signals RpmDb listens to. */
  struct SignalRecorder : public callback::ReceiveReport<KeyRingSignals>
  {
    SignalRecorder()  { connect(); }
    ~SignalRecorder() { disconnect(); }
    void trustedKeyAdded( const PublicKey & key_r ) override   { _added.push_back( key_r ); }
    void trustedKeyRemoved( const PublicKey & key_r ) override { _removed.push_back( key_r ); }
    std::vector<PublicKey> _added;
    std::vector<PublicKey> _removed;
  };

  /** What happens if \a trusted_r is trusted (as at startup) and \a found_r is then found in the general ring. */
  struct FoundResult
  {
    std::vector<PublicKeyData> sentToRpm;	///< the key data passed with each trustedKeyAdded signal
    std::vector<PublicKeyData> sentToRpmFile;	///< the same, read back from the file rpm imports
    PublicKeyData trusted;			///< the key in the trusted ring afterwards
  };

  FoundResult trustThenFind( const PublicKey & trusted_r, const PublicKey & found_r )
  {
    TmpDir tmp_dir;
    KeyRing keyring( tmp_dir.path() );
    SignalRecorder receiver;
    keyring.multiKeyImport( trusted_r.path(), true );	// silent, like RpmDb::syncTrustedKeys
    keyring.importKey( found_r, false );

    FoundResult ret;
    for ( const PublicKey & key : receiver._added ) {
      ret.sentToRpm.push_back( key.keyData() );
      ret.sentToRpmFile.push_back( PublicKey( key.path() ).keyData() );	// before tmp_dir is gone
    }
    ret.trusted = keyring.trustedPublicKeyData( trusted_r.id() );
    return ret;
  }

  /** Expiry of the subkey \a subkeyId_r of \a key_r. */
  Date subkeyExpires( const PublicKeyData & key_r, const std::string & subkeyId_r )
  {
    for ( const PublicSubkeyData & sub : key_r.subkeys() ) {
      if ( sub.id() == subkeyId_r )
        return sub.expires();
    }
    BOOST_FAIL( "no subkey " << subkeyId_r << " in " << key_r );
    return Date();
  }

  /** Check that \a res keeps the subkey \a subkeyId_r at \a expires_r: in the trusted ring and in
   * whatever was passed on to rpm. An older version of a key may be passed on, but its older
   * binding signature must not win.
   */
  void checkSubkeyExpiry( const FoundResult & res, const std::string & subkeyId_r, const Date & expires_r )
  {
    BOOST_CHECK_EQUAL( subkeyExpires( res.trusted, subkeyId_r ), expires_r );
    for ( const PublicKeyData & sent : res.sentToRpm )
      BOOST_CHECK_EQUAL( subkeyExpires( sent, subkeyId_r ), expires_r );
    for ( const PublicKeyData & sent : res.sentToRpmFile )
      BOOST_CHECK_EQUAL( subkeyExpires( sent, subkeyId_r ), expires_r );
  }
}

// bsc#1284494: A renewed trusted key imported into the general ring must update
// the trusted ring AND be announced via trustedKeyAdded, so RpmDb updates rpm.
BOOST_AUTO_TEST_CASE(keyring_renewed_trusted_key_signals_rpm)
{
  using Receiver = SignalRecorder;

  PublicKey oldKey( DATADIR + "renew-old.asc" );   // sig 2025-01-01, expired 2025-06-01
  PublicKey newKey( DATADIR + "renew-new.asc" );   // same key, sig 2025-05-01, expires 2027-01-01
  BOOST_REQUIRE_EQUAL( oldKey.fingerprint(), newKey.fingerprint() );
  BOOST_REQUIRE_EQUAL( oldKey.gpgPubkeyRelease(), "67748580" );
  BOOST_REQUIRE_EQUAL( newKey.gpgPubkeyRelease(), "6812b980" );
  BOOST_REQUIRE( newKey.keyData().isUpdateFor( oldKey.keyData() ) );

  // Renewed key in its own file
  {
    TmpDir tmp_dir;
    KeyRing keyring( tmp_dir.path() );
    Receiver receiver;

    // Like RpmDb::syncTrustedKeys at startup: silent import, no signal.
    keyring.multiKeyImport( DATADIR + "renew-old.asc", true );
    BOOST_CHECK_EQUAL( receiver._added.size(), 0 );
    BOOST_CHECK_EQUAL( keyring.trustedPublicKeyData( oldKey.id() ).created(), oldKey.created() );

    // Like fetchGpgKeys during refresh: repo key goes into the general ring.
    keyring.importKey( newKey, false );

    BOOST_CHECK_EQUAL( keyring.trustedPublicKeyData( newKey.id() ).created(), newKey.created() );

    BOOST_REQUIRE_EQUAL( receiver._added.size(), 1 );
    const PublicKey & sent { receiver._added.front() };
    BOOST_CHECK_EQUAL( sent.fingerprint(), newKey.fingerprint() );
    BOOST_CHECK_EQUAL( sent.gpgPubkeyRelease(), "6812b980" );
    BOOST_CHECK( PathInfo( sent.path() ).isFile() );
    BOOST_CHECK( PublicKey( sent.path() ).hiddenKeys().empty() );
    BOOST_CHECK( receiver._removed.empty() );

    // Importing the same key again: nothing left to update, no further signal.
    keyring.importKey( newKey, false );
    BOOST_CHECK_EQUAL( receiver._added.size(), 1 );
  }

  // Renewed key hidden in a multi key file: only the renewed key may reach rpm
  {
    TmpDir tmp_dir;
    KeyRing keyring( tmp_dir.path() );
    Receiver receiver;

    PublicKey multi( DATADIR + "renew-new-plus-other.asc" );
    BOOST_REQUIRE_EQUAL( multi.hiddenKeys().size(), 1 );   // main key is 'other'

    keyring.multiKeyImport( DATADIR + "renew-old.asc", true );
    keyring.importKey( multi, false );

    BOOST_REQUIRE_EQUAL( receiver._added.size(), 1 );
    BOOST_CHECK_EQUAL( receiver._added.front().fingerprint(), newKey.fingerprint() );
    BOOST_CHECK( PublicKey( receiver._added.front().path() ).hiddenKeys().empty() );
    BOOST_CHECK( ! keyring.isKeyTrusted( multi.id() ) );   // 'other' must stay untrusted
    BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), 1 );
  }
}

// bsc#1284494: A renewal that only extends the lifetime of a signing subkey leaves the creation date
// of the key (the user id self-signature) untouched. It must be passed on to rpm as well, because
// rpm refuses packages signed by an expired subkey.
BOOST_AUTO_TEST_CASE(keyring_renewed_subkey_signals_rpm)
{
  PublicKey oldKey( DATADIR + "renew-sub-old.asc" );   // subkey expires 2025-06-01, primary key never expires
  PublicKey newKey( DATADIR + "renew-sub-new.asc" );   // same key, subkey expires 2099-12-31
  BOOST_REQUIRE_EQUAL( oldKey.fingerprint(), newKey.fingerprint() );
  BOOST_REQUIRE_EQUAL( oldKey.expires(), Date() );
  BOOST_REQUIRE_EQUAL( newKey.expires(), Date() );
  BOOST_REQUIRE_EQUAL( oldKey.created(), newKey.created() );	// the creation date does not tell...
  BOOST_REQUIRE( ! newKey.keyData().isUpdateFor( oldKey.keyData() ) );

  auto subkeyExpires = []( const PublicKeyData & key_r ) -> Date {
    BOOST_REQUIRE( key_r.hasSubkeys() );
    return (*key_r.subkeys().begin()).expires();
  };
  BOOST_REQUIRE( subkeyExpires( oldKey.keyData() ) < subkeyExpires( newKey.keyData() ) );

  TmpDir tmp_dir;
  KeyRing keyring( tmp_dir.path() );
  SignalRecorder receiver;

  // Like RpmDb::syncTrustedKeys at startup: silent import, no signal.
  keyring.multiKeyImport( DATADIR + "renew-sub-old.asc", true );
  BOOST_CHECK_EQUAL( receiver._added.size(), 0 );
  BOOST_CHECK_EQUAL( subkeyExpires( keyring.trustedPublicKeyData( oldKey.id() ) ), subkeyExpires( oldKey.keyData() ) );

  // The renewed key is found in the general ring...
  keyring.importKey( newKey, false );

  // ...updates the trusted ring and is passed on to rpm
  BOOST_CHECK_EQUAL( subkeyExpires( keyring.trustedPublicKeyData( newKey.id() ) ), subkeyExpires( newKey.keyData() ) );
  BOOST_REQUIRE_EQUAL( receiver._added.size(), 1 );
  BOOST_CHECK_EQUAL( receiver._added.front().fingerprint(), newKey.fingerprint() );
  BOOST_CHECK( PublicKey( receiver._added.front().path() ).hiddenKeys().empty() );

  // Importing the same key again: nothing left to update, no further signal.
  keyring.importKey( newKey, false );
  BOOST_CHECK_EQUAL( receiver._added.size(), 1 );

  // The other way round: the old key shows up again. It may be passed on, but must not undo the renewal.
  const std::string sub { (*newKey.subkeys().begin()).id() };
  checkSubkeyExpiry( trustThenFind( newKey, oldKey ), sub, subkeyExpires( newKey.keyData() ) );
}

// bsc#1284494: The old and the renewed key need not have the same subkeys. Subkeys are matched by id,
// only a changed expiry of a subkey both have counts. Added subkeys (B) or subkeys the renewed key
// lacks do not matter. All files are the same key at different points in time:
//   a: subkey A (expired 2025-06-01)     ab: A and B       ab-ext: A extended, and B
//   a-ext: A extended, B never added
BOOST_AUTO_TEST_CASE(keyring_renewed_subkey_different_subkey_sets)
{
  auto load = []( const char * name_r ) { return PublicKey( DATADIR + name_r ); };
  const PublicKey a( load( "renew-sub2-a.asc" ) ), ab( load( "renew-sub2-ab.asc" ) ),
                  abExt( load( "renew-sub2-ab-ext.asc" ) ), aExt( load( "renew-sub2-a-ext.asc" ) );
  for ( const PublicKey * k : { &ab, &abExt, &aExt } ) {
    BOOST_REQUIRE_EQUAL( k->fingerprint(), a.fingerprint() );
    BOOST_REQUIRE_EQUAL( k->created(), a.created() );   // only subkey data differs
  }
  BOOST_REQUIRE_EQUAL( a.subkeys().size(), 1 );
  BOOST_REQUIRE_EQUAL( ab.subkeys().size(), 2 );
  BOOST_REQUIRE_EQUAL( abExt.subkeys().size(), 2 );
  BOOST_REQUIRE_EQUAL( aExt.subkeys().size(), 1 );

  // Whether trusting 'ring' and then finding 'found' in the general ring is passed on to rpm.
  auto passedOn = []( const PublicKey & ring_r, const PublicKey & found_r ) -> bool {
    const FoundResult res { trustThenFind( ring_r, found_r ) };
    BOOST_REQUIRE( res.sentToRpm.size() <= 1 );
    return res.sentToRpm.size() == 1;
  };

  //                  trusted   found
  BOOST_CHECK(   passedOn( a,     abExt ) );   // renewed A, and B was added: more subkeys
  BOOST_CHECK(   passedOn( ab,    aExt  ) );   // renewed A, and B is missing: fewer subkeys
  BOOST_CHECK(   passedOn( ab,    abExt ) );   // same subkeys
  BOOST_CHECK(   passedOn( a,     aExt  ) );   // same subkeys

  BOOST_CHECK( ! passedOn( a,     ab    ) );   // only B added: nothing was renewed
  BOOST_CHECK( ! passedOn( ab,    a     ) );   // B missing: nothing was renewed
  BOOST_CHECK( ! passedOn( abExt, aExt  ) );   // A equal, B missing

  // The other way round: an older version of the key shows up again. Its expiry of A differs, so it
  // may be passed on, but the renewal of A must stay.
  const std::string subA { (*a.subkeys().begin()).id() };
  const Date renewedA { subkeyExpires( aExt.keyData(), subA ) };
  BOOST_REQUIRE( subkeyExpires( a.keyData(), subA ) < renewedA );
  checkSubkeyExpiry( trustThenFind( abExt, ab ), subA, renewedA );
  checkSubkeyExpiry( trustThenFind( aExt,  a  ), subA, renewedA );
}

// A subkey lifetime can also be shortened: a newer binding signature with an earlier expiry date. That
// must be passed on as well. And an old version of the key showing up again must not bring the old
// lifetime back: the newest binding signature wins, not the latest expiry date.
BOOST_AUTO_TEST_CASE(keyring_shortened_subkey)
{
  const PublicKey oldKey( DATADIR + "renew-subshort-old.asc" );   // the subkey never expires
  const PublicKey newKey( DATADIR + "renew-subshort-new.asc" );   // newer binding: expires 2026-12-31
  BOOST_REQUIRE_EQUAL( oldKey.fingerprint(), newKey.fingerprint() );
  BOOST_REQUIRE_EQUAL( oldKey.created(), newKey.created() );     // the creation date does not tell
  BOOST_REQUIRE( oldKey.hasSubkeys() );
  const std::string sub { (*oldKey.subkeys().begin()).id() };
  const Date shortened { subkeyExpires( newKey.keyData(), sub ) };
  BOOST_REQUIRE_EQUAL( subkeyExpires( oldKey.keyData(), sub ), Date() );   // never
  BOOST_REQUIRE( shortened != Date() );

  // The shortened lifetime reaches the trusted ring and rpm.
  {
    const FoundResult res { trustThenFind( oldKey, newKey ) };
    BOOST_REQUIRE_EQUAL( res.sentToRpm.size(), 1 );
    checkSubkeyExpiry( res, sub, shortened );
  }
  // The old version of the key shows up again: the old binding signature must not win.
  checkSubkeyExpiry( trustThenFind( newKey, oldKey ), sub, shortened );
}

// The primary key and the subkey were renewed in two diverged copies of the key. The file with the
// renewed subkey has an older user id self-signature than the trusted key. The renewal must still be
// passed on, and the merged key keeps the newest signature of each part.
BOOST_AUTO_TEST_CASE(keyring_subkey_renewed_in_other_copy)
{
  const PublicKey primaryKey( DATADIR + "renew-subbranch-primary.asc" );   // newer user id self-signature
  const PublicKey subkeyKey( DATADIR + "renew-subbranch-subkey.asc" );     // newer subkey binding
  BOOST_REQUIRE_EQUAL( primaryKey.fingerprint(), subkeyKey.fingerprint() );
  BOOST_REQUIRE( subkeyKey.created() < primaryKey.created() );
  BOOST_REQUIRE( primaryKey.hasSubkeys() );
  const std::string sub { (*primaryKey.subkeys().begin()).id() };
  const Date renewed { subkeyExpires( subkeyKey.keyData(), sub ) };
  BOOST_REQUIRE( subkeyExpires( primaryKey.keyData(), sub ) < renewed );

  for ( const auto & [trusted, found] : { std::make_pair( primaryKey, subkeyKey ), std::make_pair( subkeyKey, primaryKey ) } )
  {
    const FoundResult res { trustThenFind( trusted, found ) };
    BOOST_REQUIRE_EQUAL( res.sentToRpm.size(), 1 );
    checkSubkeyExpiry( res, sub, renewed );
    BOOST_CHECK_EQUAL( res.trusted.created(), primaryKey.created() );
    BOOST_CHECK_EQUAL( res.sentToRpm.front().created(), primaryKey.created() );
    BOOST_CHECK_EQUAL( res.sentToRpmFile.front().created(), primaryKey.created() );
  }
}

BOOST_AUTO_TEST_CASE(keyring_delete)
{
  PublicKey key( Pathname(DATADIR) + "public.asc" );
  /** scenario #3
  * import and delete untrusted key
  */
  {
    // base sandbox for playing
    TmpDir tmp_dir;
    KeyRing keyring( tmp_dir.path() );

    BOOST_CHECK_EQUAL( keyring.publicKeys().size(), (unsigned) 0 );
    BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), (unsigned) 0 );

    keyring.importKey( key, false );

    BOOST_CHECK_EQUAL( keyring.publicKeys().size(), (unsigned) 1 );
    BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), (unsigned) 0 );

    keyring.deleteKey( key.id(), false);

    BOOST_CHECK_EQUAL( keyring.publicKeys().size(), (unsigned) 0 );
    BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), (unsigned) 0 );
  }

  /** scenario #3.1
  * import and delete trusted key
  */
  {
    // base sandbox for playing
    TmpDir tmp_dir;
    KeyRing keyring( tmp_dir.path() );

    BOOST_CHECK_EQUAL( keyring.publicKeys().size(), (unsigned) 0 );
    BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), (unsigned) 0 );

    keyring.importKey( key, true );

    BOOST_CHECK_EQUAL( keyring.publicKeys().size(), (unsigned) 0 );
    BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), (unsigned) 1 );

    //try to delete from untrusted keyring
    keyring.deleteKey( key.id(), false);

    BOOST_CHECK_EQUAL( keyring.publicKeys().size(), (unsigned) 0 );
    BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), (unsigned) 1 );

    keyring.deleteKey( key.id(), true);

    BOOST_CHECK_EQUAL( keyring.publicKeys().size(), (unsigned) 0 );
    BOOST_CHECK_EQUAL( keyring.trustedPublicKeys().size(), (unsigned) 0 );
  }
}


// bsc#1289557: decide when a trusted key is refreshed from its repo before install.
BOOST_AUTO_TEST_CASE(keyrefresh_needs_refresh)
{
  PublicKey oldKey( DATADIR + "renew-old.asc" );   // expired 2025-06-01
  PublicKey newKey( DATADIR + "renew-new.asc" );   // expires 2027-01-01
  PublicKey multi( DATADIR + "renew-new-plus-other.asc" );   // main key 'other' never expires

  const Date mid2026 { 1780272000 };   // 2026-06-01 UTC, from: date -ud 2026-06-01 +%s
  BOOST_CHECK(   target::keyNeedsRefresh( oldKey.keyData(), oldKey.id(), mid2026 ) );
  BOOST_CHECK( ! target::keyNeedsRefresh( newKey.keyData(), newKey.id(), mid2026 ) );
  BOOST_CHECK(   target::keyNeedsRefresh( newKey.keyData(), newKey.id(), Date( newKey.expires() - 10*Date::day ) ) );
  BOOST_CHECK( ! target::keyNeedsRefresh( newKey.keyData(), newKey.id(), Date( newKey.expires() - 20*Date::day ) ) );
  BOOST_REQUIRE_EQUAL( multi.expires(), Date() );
  BOOST_CHECK( ! target::keyNeedsRefresh( multi.keyData(), multi.id(), mid2026 ) );
}

// bsc#1289557: rpm refuses a signature made by an expired signing subkey even if the primary key
// never expires. The expiry of the subkey that signed must count.
BOOST_AUTO_TEST_CASE(keyrefresh_needs_refresh_subkey)
{
  PublicKey oldKey( DATADIR + "renew-sub-old.asc" );   // primary key never expires, subkey expires 2025-06-01
  PublicKey newKey( DATADIR + "renew-sub-new.asc" );   // primary key never expires, subkey expires 2099-12-31
  BOOST_REQUIRE_EQUAL( oldKey.expires(), Date() );
  BOOST_REQUIRE( oldKey.hasSubkeys() && newKey.hasSubkeys() );
  const std::string subId { (*oldKey.subkeys().begin()).id() };
  const Date subExpires { (*newKey.subkeys().begin()).expires() };
  BOOST_REQUIRE( subId != oldKey.id() );

  const Date mid2026 { 1780272000 };   // 2026-06-01 UTC, from: date -ud 2026-06-01 +%s

  // Signed by the subkey: the subkey decides.
  BOOST_CHECK(   target::keyNeedsRefresh( oldKey.keyData(), subId, mid2026 ) );
  BOOST_CHECK( ! target::keyNeedsRefresh( newKey.keyData(), subId, mid2026 ) );
  BOOST_CHECK(   target::keyNeedsRefresh( newKey.keyData(), subId, Date( subExpires - 10*Date::day ) ) );
  BOOST_CHECK( ! target::keyNeedsRefresh( newKey.keyData(), subId, Date( subExpires - 20*Date::day ) ) );
  BOOST_CHECK(   target::keyNeedsRefresh( oldKey.keyData(), subId.substr( 8 ), mid2026 ) );	// short key id

  // Signed by the primary key or an unknown id: the subkey does not matter.
  BOOST_CHECK( ! target::keyNeedsRefresh( oldKey.keyData(), oldKey.id(), mid2026 ) );
  BOOST_CHECK( ! target::keyNeedsRefresh( oldKey.keyData(), "0123456789ABCDEF", mid2026 ) );
  BOOST_CHECK( ! target::keyNeedsRefresh( oldKey.keyData(), "", mid2026 ) );
}

BOOST_AUTO_TEST_CASE(keyrefresh_tracker)
{
  target::KeyRefreshTracker t;
  BOOST_CHECK(   t.firstTime( "ABCD", "repoA" ) );
  BOOST_CHECK( ! t.firstTime( "ABCD", "repoA" ) );   // same pair: only once
  BOOST_CHECK( ! t.firstTime( "abcd", "repoA" ) );   // case-insensitive key id
  BOOST_CHECK(   t.firstTime( "ABCD", "repoB" ) );   // same key, other repo: try again
}
