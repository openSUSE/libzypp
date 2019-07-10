/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "zypp/KeyManager.h"
#include "zypp/PublicKey.h"
#include "zypp/PathInfo.h"
#include "zypp/base/Logger.h"
#include "zypp/TmpPath.h"
#include "zypp/base/String.h"

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

    //using boost::interprocess pointer because it allows a custom deleter
    typedef boost::interprocess::scoped_ptr<gpgme_data, boost::function<void (gpgme_data_t)>> GpgmeDataPtr;
    typedef boost::interprocess::scoped_ptr<_gpgme_key, boost::function<void (gpgme_key_t)>>  GpgmeKeyPtr;
    typedef boost::interprocess::scoped_ptr<FILE, boost::function<int (FILE *)>> FILEPtr;

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

    /** \relates gpgme_import_result_t Stream output. */
    std::ostream & operator<<( std::ostream & str, const _gpgme_op_import_result & obj )
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
  } // namespace
  ///////////////////////////////////////////////////////////////////

class KeyManagerCtx::Impl
{
public:
  Impl();
  ~Impl();

  /** Return all fingerprints found in \a signature_r. */
  std::list<std::string> readSignaturesFprs( const Pathname & signature_r )
  { return readSignaturesFprsOptVerify( signature_r ); }

  /** Tries to verify the \a file_r using \a signature_r. */
  bool verifySignaturesFprs( const Pathname & file_r, const Pathname & signature_r )
  {
    bool verify = false;
    readSignaturesFprsOptVerify( signature_r, file_r, &verify );
    return verify;
  }

  gpgme_ctx_t _ctx;

private:
  /** Return all fingerprints found in \a signature_r and optionally verify the \a file_r on the fly.
   *
   * If \a verify_r is not a \c nullptr, log verification errors and return
   * whether all signatures are good.
   */
  std::list<std::string> readSignaturesFprsOptVerify( const Pathname & signature_r, const Pathname & file_r = "/dev/null", bool * verify_r = nullptr );
};

KeyManagerCtx::Impl::Impl()
{ }


KeyManagerCtx::KeyManagerCtx()
  : _pimpl( new Impl )
{

}

std::list<std::string> KeyManagerCtx::Impl::readSignaturesFprsOptVerify( const Pathname & signature_r, const Pathname & file_r, bool * verify_r )
{
  //lets be pessimistic
  if ( verify_r )
    *verify_r = false;


  if (!PathInfo( signature_r ).isExist())
    return std::list<std::string>();

  FILEPtr dataFile(fopen(file_r.c_str(), "rb"), fclose);
  if (!dataFile)
    return std::list<std::string>();

  GpgmeDataPtr fileData(nullptr, gpgme_data_release);
  GpgmeErr err = gpgme_data_new_from_stream (&fileData.get(), dataFile.get());
  if (err) {
    ERR << err << endl;
    return std::list<std::string>();
  }

  FILEPtr sigFile(fopen(signature_r.c_str(), "rb"), fclose);
  if (!sigFile) {
    ERR << "Unable to open signature file '" << signature_r << "'" <<endl;
    return std::list<std::string>();
  }

  GpgmeDataPtr sigData(nullptr, gpgme_data_release);
  err = gpgme_data_new_from_stream (&sigData.get(), sigFile.get());
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
  std::list<std::string> signatures;
  for ( gpgme_signature_t sig = res->signatures; sig; sig = sig->next ) {

    if ( sig->fpr )
    {
      // bsc#1100427: With ibgpgme11-1.11.0 and if a recent gpg version was used
      // to create the signature, the field may contain the full fingerprint, but
      // we're expected to return the ID.
      // [https://github.com/gpg/gpgme/commit/478d1650bbef84958ccce439fac982ef57b16cd0]
      std::string id( sig->fpr );
      if ( id.size() > 16 )
	id = id.substr( id.size()-16 );
      signatures.push_back( std::move(id) );
    }

    if ( sig->status != GPG_ERR_NO_ERROR )
    {
      if ( gpgme_err_code(sig->status) != GPG_ERR_KEY_EXPIRED )
      {
	if ( !foundBadSignature )
	  foundBadSignature = true;
	if ( verify_r )
	  WAR << "Failed signature check: " << file_r << " " << GpgmeErr(sig->status) << endl;
      }
      else
      {
	if ( verify_r )
	  WAR << "Legacy: Ignore expired key: " << file_r << " " << GpgmeErr(sig->status) << endl;
      }
    }
  }

  if ( verify_r )
    *verify_r = (!foundBadSignature);
  return signatures;
}

KeyManagerCtx::Impl::~Impl()
{
  gpgme_release(_ctx);
}

KeyManagerCtx::Ptr KeyManagerCtx::createForOpenPGP()
{
  //make sure gpgpme is initialized
  boost::call_once(gpgme_init_once, initGpgme);

  gpgme_ctx_t ctx;
  GpgmeErr err = gpgme_new(&ctx);
  if (err != GPG_ERR_NO_ERROR) {
    ERR << err << endl;
    return shared_ptr<KeyManagerCtx>();
  }

  //use OpenPGP
  err = gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);
  if (err != GPG_ERR_NO_ERROR) {
    ERR << err << endl;
    gpgme_release(ctx);
    return shared_ptr<KeyManagerCtx>();
  }

  shared_ptr<KeyManagerCtx> me( new KeyManagerCtx());
  me->_pimpl->_ctx = ctx;
  return me;
}

bool KeyManagerCtx::setHomedir(const Pathname &keyring_r)
{

  /* get engine information to read current state*/
  gpgme_engine_info_t enginfo = gpgme_ctx_get_engine_info(_pimpl->_ctx);
  if (!enginfo)
    return false;

  GpgmeErr err = gpgme_ctx_set_engine_info(
        _pimpl->_ctx,
        GPGME_PROTOCOL_OpenPGP,
        enginfo->file_name,
        keyring_r.c_str());

  if (err != GPG_ERR_NO_ERROR) {
    ERR << "Unable to set homedir " << err << endl;
    return false;
  }

  return true;
}

Pathname KeyManagerCtx::homedir() const
{
  gpgme_engine_info_t enginfo = gpgme_ctx_get_engine_info(_pimpl->_ctx);
  if (!enginfo)
    return Pathname();

  return Pathname(enginfo->home_dir);
}

std::list<PublicKeyData> KeyManagerCtx::listKeys()
{
  std::list<PublicKeyData> keys;
  gpgme_key_t key;
  GpgmeErr err = GPG_ERR_NO_ERROR;

  gpgme_keylist_mode_t mode = GPGME_KEYLIST_MODE_LOCAL | GPGME_KEYLIST_MODE_SIGS;
  gpgme_set_keylist_mode (_pimpl->_ctx, mode);
  gpgme_op_keylist_start (_pimpl->_ctx, NULL, 0);

  while (!(err = gpgme_op_keylist_next(_pimpl->_ctx, &key))) {
    PublicKeyData data = PublicKeyData::fromGpgmeKey(key);
    if (data) {
      keys.push_back(data);
    }
    gpgme_key_release(key);
  }
  gpgme_op_keylist_end(_pimpl->_ctx);
  return keys;
}

std::list<PublicKeyData> KeyManagerCtx::readKeyFromFile(const Pathname &file)
{
  //seems GPGME does not support reading keys from a keyfile using
  //gpgme_data_t and gpgme_op_keylist_from_data_start, this always
  //return unsupported errors. However importing and listing the key works.
  zypp::Pathname realHomedir = homedir();

  zypp::filesystem::TmpDir tmpKeyring;
  if (!setHomedir(tmpKeyring.path()))
    return std::list<PublicKeyData>();

  if (!importKey(file)) {
    setHomedir(realHomedir);
    return std::list<PublicKeyData>();
  }

  std::list<PublicKeyData> keys = listKeys();
  setHomedir(realHomedir);
  return keys;
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
  gpgme_key_t key;
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
  err = gpgme_op_export_keys (_pimpl->_ctx, keyarray, 0, out.get());
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

  err = gpgme_op_import(_pimpl->_ctx, data.get());
  if (err) {
    ERR << "Error importing key: "<< err << endl;
    return false;
  }

  // Work around bsc#1127220 [libgpgme] no error upon incomplete import due to signal received.
  // We need this error, otherwise RpmDb will report the missing keys as 'probably v3'.
  if ( gpgme_import_result_t res = gpgme_op_import_result(_pimpl->_ctx) )
  {
    if ( ! res->considered && PathInfo(keyfile).size() )
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
  gpgme_key_t key;
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

} // namespace zypp
///////////////////////////////////////////////////////////////////
