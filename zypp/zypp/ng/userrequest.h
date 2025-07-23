/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_USERREQUEST_H_INCLUDED
#define ZYPP_NG_USERREQUEST_H_INCLUDED

#include <zypp-common/PublicKey.h>
#include <zypp/KeyContext.h>
#include <zypp-core/zyppng/ui/UserRequest>

namespace zyppng {

  /*!
   * A digest user request asking the user to accept
   * verification of a file that has no checksum available.
   *
   * Something like: "File foo.bar has no checksum. Continue?"
   *
   * Userdata fields:
   * - "file"   Path of the file to be validated
   */
  namespace AcceptNoDigestRequest {
    constexpr std::string_view CTYPE ("digest/accept-no-digest");
    constexpr std::string_view FILE  ("file");
    UserData makeData ( const zypp::Pathname &p);
  }

  /*!
   * A digest user request asking the user to accept
   * verification with a unknown digest.
   *
   * Something like: "Unknown digest <digestname> for file <file>."
   *
   * Userdata fields:
   * - "file"   Path of the file to be validated
   * - "name"   Name of the digest
   */
  namespace AcceptUnknownDigestRequest {
    constexpr std::string_view CTYPE ("digest/accept-unknown-digest");
    constexpr std::string_view FILE  ("file");
    constexpr std::string_view NAME  ("name");
    UserData makeData ( const zypp::Pathname &p, const std::string &name );
  }

  /*!
   * A digest user request asking the user to accept
   * a wrong digest.
   *
   * Something like: "Unknown digest <digestname> for file <file>."
   *
   * Userdata fields:
   * - "file"   Path of the file to be validated
   * - "name"   Name of the digest
   */
  namespace AcceptWrongDigestRequest {
    constexpr std::string_view CTYPE ("digest/accept-unknown-digest");
    constexpr std::string_view FILE  ("file");
    constexpr std::string_view NAME_REQUESTED  ("requested");
    constexpr std::string_view NAME_FOUND      ("found");
    UserData makeData ( const zypp::Pathname &p, const std::string &requested, const std::string &found );
  }


  ZYPP_FWD_DECL_TYPE_WITH_REFS(TrustKeyRequest);
  class TrustKeyRequest : public UserRequest
  {
    ZYPP_ADD_CREATE_FUNC(TrustKeyRequest)
  public:
    /*!
     * Keep in sync with zypp::KeyRingReport::KeyTrust, we do not want
     * to include old APIs here if possible.
     */
    enum KeyTrust
    {
      /**
       * User has chosen not to trust the key.
       */
      KEY_DONT_TRUST = 0,
      /**
       * This basically means, we knew the key, but it was not trusted. User
       * has chosen to continue, but not import the key.
       */
      KEY_TRUST_TEMPORARILY,
      /**
       * Import the key.
       * This means saving the key in the trusted database so next run it will appear as trusted.
       * Nothing to do with KEY_TRUST_TEMPORARILY, as you CAN trust a key without importing it,
       * basically you will be asked every time again.
       * There are programs who prefer to manage the trust keyring on their own and use trustKey
       * without importing it into rpm.
       */
      KEY_TRUST_AND_IMPORT
    };

    ZYPP_DECL_PRIVATE_CONSTR_ARGS(TrustKeyRequest, std::string label, KeyTrust trust = KEY_DONT_TRUST, UserData userData = {} );

    void setChoice ( const KeyTrust sel );
    KeyTrust choice() const;

    const std::string label() const;

    UserRequestType type() const override;

  private:
      std::string _label;
      KeyTrust _answer = KEY_DONT_TRUST;
  };

  /*!
   * Ask user to trust and/or import the key to trusted keyring.
   * \see zypp::KeyTrust
   *
   * Userdata fields:
   * - "key"          Key to import into the trusted keyring
   * - "key-context"  context the key is used in
   */
  namespace AcceptKeyRequest {
    constexpr std::string_view CTYPE ("keyring/accept-key");
    constexpr std::string_view KEY ("key");
    constexpr std::string_view KEY_CONTEXT("key-context");
    UserData makeData ( const zypp::PublicKey &key, const zypp::KeyContext &keycontext = zypp::KeyContext() );
  }


  /*!
   * Informal event showing the trusted key that will be used for verification.
   *
   * Userdata fields:
   * - "file"         File that was verified
   * - "key-data"     Key data that was used to verify the file
   * - "key-context"  context the key is used in
   */
  namespace VerifyInfoEvent {
    constexpr std::string_view CTYPE ("keyring/info-verify");
    constexpr std::string_view FILE  ("file");
    constexpr std::string_view KEY_DATA("key-data");
    constexpr std::string_view KEY_CONTEXT("key-context");
    UserData makeData ( const std::string & file_r, const zypp::PublicKeyData & keyData_r, const zypp::KeyContext &keycontext = zypp::KeyContext() );
  }

  /*!
   * Ask user to accept or reject a unsigned file
   *
   * Userdata fields:
   * - "file"         The file zypp needs to be accepted or rejected
   * - "key-context"  context the file came up in
   */
  namespace AcceptUnsignedFileRequest {
    constexpr std::string_view CTYPE ("keyring/accept-unsigned-file");
    constexpr std::string_view FILE  ("file");
    constexpr std::string_view KEY_CONTEXT("key-context");
    UserData makeData ( const std::string &file, const zypp::KeyContext &keycontext = zypp::KeyContext() );
  }

  /**
   * Ask the user to accept a unknown key.
   * We DONT know the key, only its id, but we have never seen it, the difference
   * with trust key is that if you dont have it, you can't import it later.
   * The answer means continue yes or no?
   *
   * Userdata fields:
   * - "keyid"        The key ID in question
   * - "file"         The file signed with the given key id
   * - "key-context"  context the file came up in
   *
   */
  namespace AcceptUnknownKeyRequest {
    constexpr std::string_view CTYPE ("keyring/accept-unknown-key");
    constexpr std::string_view KEYID ("keyid");
    constexpr std::string_view FILE  ("file");
    constexpr std::string_view KEY_CONTEXT("key-context");
    UserData makeData ( const std::string &file, const std::string &id, const zypp::KeyContext &keycontext = zypp::KeyContext() );
  }

  /**
   * Ask the user to accept a file that is signed but where the
   * signature verification failed.
   *
   * Userdata fields:
   * - "file"         The file in question
   * - "key"          Key zypp tried to verify with
   * - "key-context"  context the file came up in
   */
  namespace AcceptFailedVerificationRequest {
    constexpr std::string_view CTYPE        ("keyring/accept-failed-verification");
    constexpr std::string_view FILE         ("file");
    constexpr std::string_view KEY          ("key");
    constexpr std::string_view KEY_CONTEXT  ("key-context");
    UserData makeData (  const std::string &file, const zypp::PublicKey &key, const zypp::KeyContext &keycontext = zypp::KeyContext() );
  }

  /**
   * Ask user to trust and/or import the package key to trusted keyring
   *
   * Userdata fields:
   * - "key"          The PublicKey to be accepted
   * - "key-context"  The KeyContext
   *
   * \see zypp::KeyTrust
   */
  namespace AcceptPackageKeyRequest {
    constexpr std::string_view CTYPE        ("keyring/accept-package-key");
    constexpr std::string_view KEY          ("key");
    constexpr std::string_view KEY_CONTEXT  ("key-context");
    UserData makeData ( const zypp::PublicKey &key_r, const zypp::KeyContext &keycontext_r = zypp::KeyContext() );
  }

  /**
   * Notify the user about keys that were not imported from the
   * rpm key database into zypp keyring
   *
   * Userdata fields:
   * const std::set<Edition> * "Keys"   set of keys that were not imported
   *
   */
  namespace NonImportedKeysInfoEvent {
    constexpr std::string_view CTYPE ("keyring/keys-not-imported");
    constexpr std::string_view KEYS  ("keys");
    UserData makeData ( const std::set<zypp::Edition> &keys_r );
  }

  /**
   * Notify that a repository auto imported new package signing keys.
   *
   * To auto import new package signing keys, the repositories metadata must be
   * signed by an already trusted key.
   *
   * Userdata fields:
   * const std::list<zypp::PublicKeyData> * "key-data-list"  List of KeyData to import
   * zypp::PublicKeyData "key-data"   KeyData of signing key
   * zypp::KeyContext "key-context"   The KeyContext
   */
  namespace KeyAutoImportInfoEvent {
    constexpr std::string_view CTYPE          ("keyring/auto-import-key-info");
    constexpr std::string_view KEY_DATA_LIST  ("key-data-list");
    constexpr std::string_view KEY_DATA       ("key-data");
    constexpr std::string_view KEY_CONTEXT    ("key-context");
    UserData makeData ( const std::list<zypp::PublicKeyData> & keyDataList_r,
                        const zypp::PublicKeyData & keySigning_r,
                        const zypp::KeyContext &keyContext_r );
  }


}

#endif // ZYPP_NG_USERREQUEST_H_INCLUDED
