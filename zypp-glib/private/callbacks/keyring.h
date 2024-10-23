/*---------------------------------------------------------------------------*\
                          ____  _ _ __ _ __  ___ _ _
                         |_ / || | '_ \ '_ \/ -_) '_|
                         /__|\_, | .__/ .__/\___|_|
                             |__/|_|  |_|
\*---------------------------------------------------------------------------*/

#ifndef ZYPP_GLIB_ZMART_KEYRINGCALLBACKS_H
#define ZYPP_GLIB_ZMART_KEYRINGCALLBACKS_H

#include <zypp/base/Logger.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/Pathname.h>
#include <zypp/KeyRing.h>
#include <zypp/Digest.h>


namespace zypp
{
    struct KeyRingReceive : public callback::ReceiveReport<KeyRingReport>
    {
      KeyRingReceive() {}

      ////////////////////////////////////////////////////////////////////

      virtual void infoVerify( const std::string & file_r, const PublicKeyData & keyData_r, const KeyContext & context = KeyContext() )
      {

      }

      ////////////////////////////////////////////////////////////////////
      virtual bool askUserToAcceptUnsignedFile(
          const std::string & file, const KeyContext & context)
      {

      }

      ////////////////////////////////////////////////////////////////////

      virtual bool askUserToAcceptUnknownKey(
          const std::string & file,
          const std::string & id,
          const KeyContext & context)
      {

      }

      virtual KeyRingReport::KeyTrust askUserToAcceptKey(
          const PublicKey &key, const KeyContext & context)
      {
        return askUserToAcceptKey( key, context, true );
      }

      KeyRingReport::KeyTrust askUserToAcceptKey(
                const PublicKey &key_r, const KeyContext & context_r, bool canTrustTemporarily_r )
      {
        return KeyRingReport::KEY_DONT_TRUST;
      }

      ////////////////////////////////////////////////////////////////////

      virtual bool askUserToAcceptVerificationFailed(
          const std::string & file,
          const PublicKey & key,
          const KeyContext & context )
      {
      }

      ////////////////////////////////////////////////////////////////////

      virtual void report ( const UserData & data )
      {
        if ( data.type() == zypp::ContentType( KeyRingReport::ACCEPT_PACKAGE_KEY_REQUEST ) )
          return askUserToAcceptPackageKey( data );
        else if ( data.type() == zypp::ContentType( KeyRingReport::KEYS_NOT_IMPORTED_REPORT ) )
          return reportKeysNotImportedReport( data );
        else if ( data.type() == zypp::ContentType( KeyRingReport::REPORT_AUTO_IMPORT_KEY ) )
          return reportAutoImportKey( data );
        WAR << "Unhandled report() call" << std::endl;
      }

      void askUserToAcceptPackageKey( const UserData & data )
      {
        if ( !data.hasvalue("PublicKey") || !data.hasvalue(("KeyContext")) ) {
          WAR << "Missing arguments in report call for content type: " << data.type() << std::endl;
          return;
        }
        const PublicKey &key  = data.get<PublicKey>("PublicKey");
        const KeyContext &ctx = data.get<KeyContext>("KeyContext");
        KeyRingReport::KeyTrust res = askUserToAcceptKey(key,ctx, false);
        data.set("TrustKey", res == KeyRingReport::KEY_TRUST_AND_IMPORT);
        return;
      }

      void reportKeysNotImportedReport( const UserData & data )
      {
        if ( !data.hasvalue("Keys") )
        {
          WAR << "Missing arguments in report call for content type: " << data.type() << endl;
          return;
        }
        Zypper & zypper = Zypper::instance();

        zypper.out().notePar(_("The rpm database seems to contain old V3 version gpg keys which are meanwhile obsolete and considered insecure:") );

        zypper.out().gap();
        for ( const Edition & ed : data.get( "Keys", std::set<Edition>() ) )
          zypper.out().info( str::Str() << /*indent8*/"        gpg-pubkey-" << ed );

        Zypper::instance().out().par( 4,
                                      str::Format(_("To see details about a key call '%1%'.") )
                                      % "rpm -qi GPG-PUBKEY-VERSION" );

        Zypper::instance().out().par( 4,
                                      str::Format(_("Unless you believe the key in question is still in use, you can remove it from the rpm database calling '%1%'.") )
                                      % "rpm -e GPG-PUBKEY-VERSION" );

        zypper.out().gap();
      }

      void reportAutoImportKey( const UserData & data_r )
      {
        if ( not ( data_r.hasvalue("KeyDataList") && data_r.hasvalue("KeySigning") && data_r.hasvalue("KeyContext") ) ) {
          WAR << "Missing arguments in report call for content type: " << data_r.type() << endl;
          return;
        }
        const std::list<PublicKeyData> & keyDataList { data_r.get<std::list<PublicKeyData>>("KeyDataList") };
        const PublicKeyData &            keySigning  { data_r.get<PublicKeyData>("KeySigning") };
        const KeyContext &               context     { data_r.get<KeyContext>("KeyContext") };

        zypper.out().par( 2,_("Those additional keys are usually used to sign packages shipped by the repository. In order to validate those packages upon download and installation the new keys will be imported into the rpm database.") );

        auto newTag { HIGHLIGHTString(_("New:") ) };
        for ( const auto & kd : keyDataList ) {
          zypper.out().gap();
          dumpKeyInfo( std::cout << "  " << newTag << endl, kd );
        }

        zypper.out().par( 2,HIGHLIGHTString(_("The repository metadata introducing the new keys have been signed and validated by the trusted key:")) );
        zypper.out().gap();
        dumpKeyInfo( std::cout, keySigning, context );

        zypper.out().gap();
      }

    private:
      const Config & _gopts;
    };

    ///////////////////////////////////////////////////////////////////
    // DigestReceive
    ///////////////////////////////////////////////////////////////////

    struct DigestReceive : public callback::ReceiveReport<DigestReport>
    {
      DigestReceive() : _gopts(Zypper::instance().config()) {}

      ////////////////////////////////////////////////////////////////////

      virtual bool askUserToAcceptNoDigest( const Pathname &file )
      {
        std::string question = (str::Format(_("No digest for file %s.")) % file).str() + " " + text::qContinue();
        return read_bool_answer(PROMPT_GPG_NO_DIGEST_ACCEPT, question, _gopts.no_gpg_checks);
      }

      ////////////////////////////////////////////////////////////////////

      virtual bool askUserToAccepUnknownDigest( const Pathname &file, const std::string &name )
      {
        std::string question = (str::Format(_("Unknown digest %s for file %s.")) %name % file).str() + " " + text::qContinue();
        return read_bool_answer(PROMPT_GPG_UNKNOWN_DIGEST_ACCEPT, question, _gopts.no_gpg_checks);
      }

      ////////////////////////////////////////////////////////////////////

      virtual bool askUserToAcceptWrongDigest( const Pathname &file, const std::string &requested, const std::string &found )
      {
        Zypper & zypper = Zypper::instance();
        std::string unblock( found.substr( 0, 4 ) );

        zypper.out().gap();
        // translators: !!! BOOST STYLE PLACEHOLDERS ( %N% - reorder and multiple occurrence is OK )
        // translators: %1%      - a file name
        // translators: %2%      - full path name
        // translators: %3%, %4% - checksum strings (>60 chars), please keep them aligned
        zypper.out().warning( str::Format(_(
                "Digest verification failed for file '%1%'\n"
                "[%2%]\n"
                "\n"
                "  expected %3%\n"
                "  but got  %4%\n" ) )
                % file.basename()
                % file
                % requested
                % found
        );

        zypper.out().info( MSG_WARNINGString(_(
                "Accepting packages with wrong checksums can lead to a corrupted system "
                "and in extreme cases even to a system compromise." ) ).str()
        );
        zypper.out().gap();

        // translators: !!! BOOST STYLE PLACEHOLDERS ( %N% - reorder and multiple occurrence is OK )
        // translators: %1%      - abbreviated checksum (4 chars)
        zypper.out().info( str::Format(_(
                "However if you made certain that the file with checksum '%1%..' is secure, correct\n"
                "and should be used within this operation, enter the first 4 characters of the checksum\n"
                "to unblock using this file on your own risk. Empty input will discard the file.\n" ) )
                % unblock
        );

        // translators: A prompt option
        PromptOptions popts( unblock+"/"+_("discard"), 1 );
        // translators: A prompt option help text
        popts.setOptionHelp( 0, _("Unblock using this file on your own risk.") );
        // translators: A prompt option help text
        popts.setOptionHelp( 1, _("Discard the file.") );
        popts.setShownCount( 1 );

        // translators: A prompt text
        zypper.out().prompt( PROMPT_GPG_WRONG_DIGEST_ACCEPT, _("Unblock or discard?"), popts );
        int reply = get_prompt_reply( zypper, PROMPT_GPG_WRONG_DIGEST_ACCEPT, popts );
        return( reply == 0 );
      }

    private:
      const Config & _gopts;
    };

   ///////////////////////////////////////////////////////////////////
}; // namespace zypp
///////////////////////////////////////////////////////////////////

class KeyRingCallbacks {

  private:
    KeyRingReceive _keyRingReport;

  public:
    KeyRingCallbacks()
    {
      _keyRingReport.connect();
    }

    ~KeyRingCallbacks()
    {
      _keyRingReport.disconnect();
    }

};

class DigestCallbacks {

  private:
    DigestReceive _digestReport;

  public:
    DigestCallbacks()
    {
      _digestReport.connect();
    }

    ~DigestCallbacks()
    {
      _digestReport.disconnect();
    }

};


#endif // ZYPP_GLIB_ZMART_KEYRINGCALLBACKS_H
// Local Variables:
// c-basic-offset: 2
// End:
