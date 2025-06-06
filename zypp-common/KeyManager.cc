/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include <iostream>
#include "KeyManager.h"
#include "KeyRingException.h"

#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/base/Logger.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/base/String.h>
#include <zypp-core/AutoDispose.h>

#include <boost/thread/once.hpp>
#include <boost/interprocess/smart_ptr/scoped_ptr.hpp>
#include <gpgme.h>

#include <stdio.h>
using std::endl;

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::gpg"

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace
  {
    // @TODO [threading]
    // make sure to call the init code of gpgme only once
    // this might need to be moved to a different location when
    // threads are introduced into libzypp
    boost::once_flag gpgme_init_once = BOOST_ONCE_INIT;

    void initGpgme ()
    {
      const char *version = gpgme_check_version(NULL);
      if ( version )
      {
        MIL << "Initialized libgpgme version: " << version << endl;
      }
      else
      {
        MIL << "Initialized libgpgme with unknown version" << endl;
      }
    }

    //using boost::interprocess pointer because it allows a custom deleter
    using GpgmeDataPtr = boost::interprocess::scoped_ptr<gpgme_data, boost::function<void (gpgme_data_t)>>;
    using GpgmeKeyPtr = boost::interprocess::scoped_ptr<_gpgme_key, boost::function<void (gpgme_key_t)>>;
    using FILEPtr = boost::interprocess::scoped_ptr<FILE, boost::function<int (FILE *)>>;

    struct GpgmeErr
    {
      GpgmeErr( gpgme_error_t err_r = GPG_ERR_NO_ERROR )
      : _err( err_r )
      {}
      operator gpgme_error_t() const { return _err; }
    private:
      gpgme_error_t _err;
    };

    std::ostream & operator<<( std::ostream & str, const GpgmeErr & obj )
    { return str << "<" << gpgme_strsource(obj) << "> " << gpgme_strerror(obj); }

    /** \relates gpgme_import_result_t Stream output. */
    [[maybe_unused]] std::ostream & operator<<( std::ostream & str, const _gpgme_op_import_result & obj )
    {
      str << "gpgme_op_import_result {" << endl;
      str << "  "  << obj.considered		<< " The total number of considered keys." << endl;
      str << "  "  << obj.no_user_id		<< " The number of keys without user ID." << endl;
      str << "  "  << obj.imported		<< " The total number of imported keys." << endl;
      str << "  "  << obj.imported_rsa		<< " imported RSA keys." << endl;
      str << "  "  << obj.unchanged		<< " unchanged keys." << endl;
      str << "  "  << obj.new_user_ids		<< " new user IDs." << endl;
      str << "  "  << obj.new_sub_keys		<< " new sub keys." << endl;
      str << "  "  << obj.new_signatures	<< " new signatures." << endl;
      str << "  "  << obj.new_revocations	<< " new revocations." << endl;
      str << "  "  << obj.secret_read		<< " secret keys read." << endl;
      str << "  "  << obj.secret_imported	<< " imported secret keys." << endl;
      str << "  "  << obj.secret_unchanged	<< " unchanged secret keys." << endl;
      str << "  "  << obj.not_imported		<< " keys not imported." << endl;
      for ( gpgme_import_status_t p = obj.imports; p; p = p->next )
      {
        str << "  - "  << p->fpr << ": " << p->result << endl;
      }
      // In V.1.11: str << "  "  << obj.skipped_v3_keys	<< " skipped v3 keys." << endl;
      return str << "}";
    }

    [[maybe_unused]] std::ostream & operator<<( std::ostream & str, const gpgme_sigsum_t & obj )
    {
      str << ((int)obj&(int)0xffff) << ":";
#define OSC(V) if ( V & (unsigned)obj ) str << " " << #V;
      OSC(GPGME_SIGSUM_VALID       );
      OSC(GPGME_SIGSUM_GREEN       );
      OSC(GPGME_SIGSUM_RED         );
      OSC(GPGME_SIGSUM_KEY_REVOKED );
      OSC(GPGME_SIGSUM_KEY_EXPIRED );
      OSC(GPGME_SIGSUM_SIG_EXPIRED );
      OSC(GPGME_SIGSUM_KEY_MISSING );
      OSC(GPGME_SIGSUM_CRL_MISSING );
      OSC(GPGME_SIGSUM_CRL_TOO_OLD );
      OSC(GPGME_SIGSUM_BAD_POLICY  );
      OSC(GPGME_SIGSUM_SYS_ERROR   );
      OSC(GPGME_SIGSUM_TOFU_CONFLICT);
#undef OSC
      return str;
    }

    [[maybe_unused]] std::ostream & operator<<( std::ostream & str, const gpgme_signature_t & obj )
    {
      str << "gpgme_signature_t " << (void *)obj << " {" << endl;
      str << "  next:            " << (void *)obj->next << endl;
      str << "  summary:         " << obj->summary << endl;
      str << "  fpr:             " << obj->fpr << endl;
      str << "  status:          " << obj->status << " " << GpgmeErr(obj->status) << endl;
      str << "  timestamp:       " << obj->timestamp << endl;
      str << "  exp_timestamp:   " << obj->exp_timestamp << endl;
      str << "  wrong_key_usage: " << obj->wrong_key_usage << endl;
      str << "  pka_trust:       " << obj->pka_trust << endl;
      str << "  chain_model:     " << obj->chain_model << endl;
      str << "  is_de_vs:        " << obj->is_de_vs << endl;
      str << "  validity:        " << obj->validity << endl;
      str << "  validity_reason: " << obj->validity_reason << " " << GpgmeErr(obj->validity_reason) << endl;
      str << "  pubkey_algo:     " << obj->pubkey_algo << endl;
      str << "  hash_algo:       " << obj->hash_algo << endl;
      str << "  pka_address:     " << (obj->pka_address ? obj->pka_address : "") << endl;
      return str;
    }

  } // namespace
  ///////////////////////////////////////////////////////////////////

  struct GpgmeException : public KeyRingException
  {
    GpgmeException( const std::string & in_r, const GpgmeErr & err_r )
    : KeyRingException( str::Format( "libgpgme error in '%1%': %2%" ) % in_r % err_r )
    {}
  };

  class KeyManagerCtx::Impl
  {
  public:
    Impl()
    { boost::call_once( gpgme_init_once, initGpgme ); }

    Impl(const Impl &) = delete;
    Impl(Impl &&) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl &operator=(Impl &&) = delete;

    ~Impl() {
      if (_ctx)
        gpgme_release(_ctx);
    }

    /** Return all fingerprints found in \a signature_r. */
    std::list<std::string> readSignaturesFprs( const Pathname & signature_r )
    { return readSignaturesFprsOptVerify( signature_r ); }

    /** Return all fingerprints found in \a signature_r. */
    std::list<std::string> readSignaturesFprs( const ByteArray & signature_r )
    { return readSignaturesFprsOptVerify( signature_r ); }

    /** Tries to verify the \a file_r using \a signature_r. */
    bool verifySignaturesFprs( const Pathname & file_r, const Pathname & signature_r )
    {
      bool verify = false;
      readSignaturesFprsOptVerify( signature_r, file_r, &verify );
      return verify;
    }

    template< typename Callback >
    bool importKey(GpgmeDataPtr &data, Callback &&calcDataSize );

    gpgme_ctx_t _ctx { nullptr };
    bool _volatile { false };	///< readKeyFromFile workaround bsc#1140670

  private:
    /** Return all fingerprints found in \a signature_r and optionally verify the \a file_r on the fly.
     *
     * If \a verify_r is not a \c nullptr, log verification errors and return
     * whether all signatures are good.
     */
    std::list<std::string> readSignaturesFprsOptVerify( const Pathname & signature_r, const Pathname & file_r = "/dev/null", bool * verify_r = nullptr );
    std::list<std::string> readSignaturesFprsOptVerify( const ByteArray& keyData_r, const Pathname & file_r = "/dev/null", bool * verify_r = nullptr );
    std::list<std::string> readSignaturesFprsOptVerify( GpgmeDataPtr &sigData, const Pathname & file_r = "/dev/null", bool * verify_r = nullptr );
  };

std::list<std::string> KeyManagerCtx::Impl::readSignaturesFprsOptVerify( const Pathname & signature_r, const Pathname & file_r, bool * verify_r )
{
  //lets be pessimistic
  if ( verify_r )
    *verify_r = false;

  if (!PathInfo( signature_r ).isExist())
    return std::list<std::string>();

  FILEPtr sigFile(fopen(signature_r.c_str(), "rb"), fclose);
  if (!sigFile) {
    ERR << "Unable to open signature file '" << signature_r << "'" <<endl;
    return std::list<std::string>();
  }

  GpgmeDataPtr sigData(nullptr, gpgme_data_release);
  GpgmeErr err = gpgme_data_new_from_stream (&sigData.get(), sigFile.get());
  if (err) {
    ERR << err << endl;
    return std::list<std::string>();
  }

  return readSignaturesFprsOptVerify( sigData, file_r, verify_r );
}

std::list<std::string> KeyManagerCtx::Impl::readSignaturesFprsOptVerify( const ByteArray &keyData_r, const filesystem::Pathname &file_r, bool *verify_r )
{
  //lets be pessimistic
  if ( verify_r )
    *verify_r = false;

  GpgmeDataPtr sigData(nullptr, gpgme_data_release);
  GpgmeErr err = gpgme_data_new_from_mem(&sigData.get(), keyData_r.data(), keyData_r.size(), 1 );
  if (err) {
    ERR << err << endl;
    return std::list<std::string>();
  }

  return readSignaturesFprsOptVerify( sigData, file_r, verify_r );
}

std::list<std::string> KeyManagerCtx::Impl::readSignaturesFprsOptVerify(GpgmeDataPtr &sigData, const filesystem::Pathname &file_r, bool *verify_r)
{
  //lets be pessimistic
  if ( verify_r )
    *verify_r = false;

  FILEPtr dataFile(fopen(file_r.c_str(), "rb"), fclose);
  if (!dataFile)
    return std::list<std::string>();

  GpgmeDataPtr fileData(nullptr, gpgme_data_release);
  GpgmeErr err = gpgme_data_new_from_stream (&fileData.get(), dataFile.get());
  if (err) {
    ERR << err << endl;
    return std::list<std::string>();
  }

  err = gpgme_op_verify(_ctx, sigData.get(), fileData.get(), NULL);
  if (err != GPG_ERR_NO_ERROR) {
    ERR << err << endl;
    return std::list<std::string>();
  }

  gpgme_verify_result_t res = gpgme_op_verify_result(_ctx);
  if (!res || !res->signatures) {
    ERR << "Unable to read signature fingerprints" <<endl;
    return std::list<std::string>();
  }

  bool foundBadSignature = false;
  bool foundGoodSignature = false;
  std::list<std::string> signatures;
  for ( gpgme_signature_t sig = res->signatures; sig; sig = sig->next ) {
    //DBG << "- " << sig << std::endl;
    if ( sig->fpr )
    {
      // bsc#1100427: With libgpgme11-1.11.0 and if a recent gpg version was used
      // to create the signature, the field may contain the full fingerprint, but
      // we're expected to return the ID.
      // [https://github.com/gpg/gpgme/commit/478d1650bbef84958ccce439fac982ef57b16cd0]
      std::string id( sig->fpr );
      if ( id.size() > 16 )
        id = id.substr( id.size()-16 );

      DBG << "Found signature with ID: " << id  << " in " << file_r << std::endl;
      signatures.push_back( std::move(id) );
    }

    if ( verify_r && sig->status != GPG_ERR_NO_ERROR ) {
      const auto status = gpgme_err_code(sig->status);

      // bsc#1180721: libgpgme started to return signatures of unknown keys, which breaks
      // our workflow when verifying files that have multiple signatures, including some that are
      // not in the trusted keyring. We should not fail if we have unknown or expired keys and at least a good one.
      // We will however keep the behaviour of failing if we find a bad signatures even if others are good.
      if ( status != GPG_ERR_KEY_EXPIRED && status != GPG_ERR_NO_PUBKEY )
      {
        WAR << "Failed signature check: " << file_r << " " << GpgmeErr(sig->status) << endl;
        if ( !foundBadSignature )
          foundBadSignature = true;
      }
      else
      {
        WAR << "Legacy: Ignore expired or unknown key: " << file_r << " " << GpgmeErr(sig->status) << endl;
        // for now treat expired keys as good signature
        if ( status == GPG_ERR_KEY_EXPIRED )
          foundGoodSignature = true;
      }
    } else {
      foundGoodSignature = true;
    }
  }

  if ( verify_r )
    *verify_r = (!foundBadSignature) && foundGoodSignature;
  return signatures;
}

KeyManagerCtx::KeyManagerCtx()
: _pimpl( new Impl )
{}

KeyManagerCtx KeyManagerCtx::createForOpenPGP()
{
  static Pathname tmppath( zypp::myTmpDir() / "PublicKey" );
  filesystem::assert_dir( tmppath );

  KeyManagerCtx ret { createForOpenPGP( tmppath ) };
  ret._pimpl->_volatile = true;	// readKeyFromFile workaround bsc#1140670
  return ret;
}

KeyManagerCtx KeyManagerCtx::createForOpenPGP( const Pathname & keyring_r )
{
  DBG << "createForOpenPGP(" << keyring_r << ")" << endl;

  KeyManagerCtx ret;
  gpgme_ctx_t & ctx { ret._pimpl->_ctx };

  // create the context
  GpgmeErr err = gpgme_new( &ctx );
  if ( err != GPG_ERR_NO_ERROR )
    ZYPP_THROW( GpgmeException( "gpgme_new", err ) );

  // use OpenPGP
  err = gpgme_set_protocol( ctx, GPGME_PROTOCOL_OpenPGP );
  if ( err != GPG_ERR_NO_ERROR )
    ZYPP_THROW( GpgmeException( "gpgme_set_protocol", err ) );

  if ( !keyring_r.empty() ) {
    // get engine information to read current state
    gpgme_engine_info_t enginfo = gpgme_ctx_get_engine_info( ctx );
    if ( !enginfo )
      ZYPP_THROW( GpgmeException( "gpgme_ctx_get_engine_info", err ) );

    err = gpgme_ctx_set_engine_info( ctx, GPGME_PROTOCOL_OpenPGP, enginfo->file_name, keyring_r.c_str() );
    if ( err != GPG_ERR_NO_ERROR )
      ZYPP_THROW( GpgmeException( "gpgme_ctx_set_engine_info", err ) );
  }
#if 0
  DBG << "createForOpenPGP {" << endl;
  for ( const auto & key : ret.listKeys() ) {
    DBG << "  " << key << endl;
  }
  DBG << "}" << endl;
#endif
  return ret;
}

Pathname KeyManagerCtx::homedir() const
{
  Pathname ret;
  if ( gpgme_engine_info_t enginfo = gpgme_ctx_get_engine_info( _pimpl->_ctx ) )
    ret = enginfo->home_dir;
  return ret;
}

std::list<PublicKeyData> KeyManagerCtx::listKeys()
{
  std::list<PublicKeyData> ret;
  GpgmeErr err = GPG_ERR_NO_ERROR;

  // Reset gpgme_keylist_mode on return!
  AutoDispose<gpgme_keylist_mode_t> guard { gpgme_get_keylist_mode( _pimpl->_ctx ), bind( &gpgme_set_keylist_mode, _pimpl->_ctx, _1 ) };
  // Let listed keys include signatures (required if PublicKeyData are created from the key)
  if ( (err = gpgme_set_keylist_mode( _pimpl->_ctx, GPGME_KEYLIST_MODE_LOCAL | GPGME_KEYLIST_MODE_SIGS )) != GPG_ERR_NO_ERROR ) {
    ERR << "gpgme_set_keylist_mode: " << err << endl;
    return ret;
  }

  if ( (err = gpgme_op_keylist_start( _pimpl->_ctx, NULL, 0 )) != GPG_ERR_NO_ERROR ) {
    ERR << "gpgme_op_keylist_start: " << err << endl;
    return ret;
  }
  // Close list operation on return!
  AutoDispose<gpgme_ctx_t> guard2 { _pimpl->_ctx, &gpgme_op_keylist_end };

  AutoDispose<gpgme_key_t> key { nullptr, &gpgme_key_release };
  for ( ; gpgme_op_keylist_next( _pimpl->_ctx, &(*key) ) == GPG_ERR_NO_ERROR; key.getDispose()( key ) ) {
    PublicKeyData data { PublicKeyData::fromGpgmeKey( key ) };
    if ( data )
      ret.push_back( data );
  }

  return ret;
}

std::list<PublicKeyData> KeyManagerCtx::readKeyFromFile( const Pathname & keyfile_r )
{
  // bsc#1140670: GPGME does not support reading keys from a keyfile using
  // gpgme_data_t and gpgme_op_keylist_from_data_start. Despite GPGME_KEYLIST_MODE_SIGS
  // the signatures are missing, but we need them to create proper PublicKeyData objects.
  // While this is not resolved, we read into a temp. keyring. Impl::_volatile helps
  // to detect whether we can clear and import into the current context or need to
  // create a temp. one.
  std::list<PublicKeyData> ret;

  if ( _pimpl->_volatile ) {
    // in a volatile context we can simple clear the keyring...
    filesystem::clean_dir( homedir() );
    if ( importKey( keyfile_r ) )
      ret = listKeys();
  } else {
    // read in a volatile context
    ret = createForOpenPGP().readKeyFromFile( keyfile_r );
  }

  return ret;
}

bool KeyManagerCtx::verify(const Pathname &file, const Pathname &signature)
{
  return _pimpl->verifySignaturesFprs(file, signature);
}

bool KeyManagerCtx::exportKey(const std::string &id, std::ostream &stream)
{
  GpgmeErr err = GPG_ERR_NO_ERROR;

  GpgmeKeyPtr foundKey;

  //search for requested key id
  gpgme_key_t key = nullptr;
  gpgme_op_keylist_start(_pimpl->_ctx, NULL, 0);
  while (!(err = gpgme_op_keylist_next(_pimpl->_ctx, &key))) {
    if (key->subkeys && id == str::asString(key->subkeys->keyid)) {
      GpgmeKeyPtr(key, gpgme_key_release).swap(foundKey);
      break;
    }
    gpgme_key_release(key);
  }
  gpgme_op_keylist_end(_pimpl->_ctx);

  if (!foundKey) {
    WAR << "Key " << id << "not found" << endl;
    return false;
  }

  //function needs a array of keys to export
  gpgme_key_t keyarray[2];
  keyarray[0] = foundKey.get();
  keyarray[1] = NULL;

  GpgmeDataPtr out(nullptr, gpgme_data_release);
  err = gpgme_data_new (&out.get());
  if (err) {
    ERR << err << endl;
    return false;
  }

  //format as ascii armored
  gpgme_set_armor (_pimpl->_ctx, 1);
  // bsc#1179222: Remove outdated self signatures when exporting the key.
  // The keyring does not order the signatures when multiple versions of the
  // same key are imported. Rpm however uses the 1st to compute the -release
  // of the gpg-pubkey. So we export only the latest to get a proper-release.
  err = gpgme_op_export_keys (_pimpl->_ctx, keyarray, GPGME_EXPORT_MODE_MINIMAL, out.get());
  if (!err) {
    int ret = gpgme_data_seek (out.get(), 0, SEEK_SET);
    if (ret) {
      ERR << "Unable to seek in exported key data" << endl;
      return false;
    }

    const int bufsize = 512;
    char buf[bufsize + 1];
    while ((ret = gpgme_data_read(out.get(), buf, bufsize)) > 0) {
      stream.write(buf, ret);
    }

    //failed to read from buffer
    if (ret < 0) {
      ERR << "Unable to read exported key data" << endl;
      return false;
    }
  } else {
    ERR << "Error exporting key: "<< err << endl;
    return false;
  }

  //if we reach this point export was successful
  return true;
}

bool KeyManagerCtx::importKey(const Pathname &keyfile)
{
  if ( !PathInfo( keyfile ).isExist() ) {
    ERR << "Keyfile '" << keyfile << "' does not exist.";
    return false;
  }

  GpgmeDataPtr data(nullptr, gpgme_data_release);
  GpgmeErr err;

  err = gpgme_data_new_from_file(&data.get(), keyfile.c_str(), 1);
  if (err) {
    ERR << "Error importing key: "<< err << endl;
    return false;
  }

  return _pimpl->importKey( data, [&](){ return PathInfo(keyfile).size(); } );
}

bool KeyManagerCtx::importKey( const ByteArray &keydata )
{
  GpgmeDataPtr data(nullptr, gpgme_data_release);
  GpgmeErr err;

  err = gpgme_data_new_from_mem( &data.get(), keydata.data(), keydata.size(), 1);
  if (err) {
    ERR << "Error importing key: "<< err << endl;
    return false;
  }

  return _pimpl->importKey( data, [&](){ return keydata.size(); } );
}

template<typename Callback>
bool KeyManagerCtx::Impl::importKey(GpgmeDataPtr &data, Callback &&calcDataSize)
{
  GpgmeErr err;
  err = gpgme_op_import( _ctx, data.get() );
  if (err) {
    ERR << "Error importing key: "<< err << endl;
    return false;
  }

  // Work around bsc#1127220 [libgpgme] no error upon incomplete import due to signal received.
  // We need this error, otherwise RpmDb will report the missing keys as 'probably v3'.
  if ( gpgme_import_result_t res = gpgme_op_import_result(_ctx) )
  {
    if ( ! res->considered && std::forward<Callback>(calcDataSize)() )
    {
      DBG << *res << endl;
      ERR << "Error importing key: No keys considered (bsc#1127220, [libgpgme] signal received?)" << endl;
      return false;
    }
  }

  return (err == GPG_ERR_NO_ERROR);
}

bool KeyManagerCtx::deleteKey(const std::string &id)
{
  gpgme_key_t key = nullptr;
  GpgmeErr err = GPG_ERR_NO_ERROR;

  gpgme_op_keylist_start(_pimpl->_ctx, NULL, 0);

  while (!(err = gpgme_op_keylist_next(_pimpl->_ctx, &key))) {
    if (key->subkeys && id == str::asString(key->subkeys->keyid)) {
        err = gpgme_op_delete(_pimpl->_ctx, key, 0);

        gpgme_key_release(key);
        gpgme_op_keylist_end(_pimpl->_ctx);

        if (err) {
          ERR << "Error deleting key: "<< err << endl;
          return false;
        }
        return true;
    }
    gpgme_key_release(key);
  }

  gpgme_op_keylist_end(_pimpl->_ctx);
  WAR << "Key: '"<< id << "' not found." << endl;
  return false;
}

std::list<std::string> KeyManagerCtx::readSignatureFingerprints(const Pathname &signature)
{ return _pimpl->readSignaturesFprs(signature); }

std::list<std::string> KeyManagerCtx::readSignatureFingerprints(const ByteArray &keyData)
{ return _pimpl->readSignaturesFprs(keyData); }

} // namespace zypp
///////////////////////////////////////////////////////////////////
