/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "reporthelper.h"

#include <zypp/Digest.h>
#include <zypp/ng/userrequest.h>

namespace zyppng {


  BasicReportHelper::BasicReportHelper(ContextRef &&ctx)
    : _ctx( std::move(ctx) )
  { }


  bool DigestReportHelper::askUserToAcceptNoDigest(const zypp::Pathname &file)
  {
#ifdef ZYPP_ENABLE_ASYNC
      std::string label = (zypp::str::Format(_("No digest for file %s.")) % file ).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptNoDigestRequest::makeData(file) );
      this->_ctx->sendUserRequest( req );
      return req->choice ();
#else
      return _report->askUserToAcceptNoDigest(file);
#endif
  }

  bool DigestReportHelper::askUserToAccepUnknownDigest(const zypp::Pathname &file, const std::string &name)
  {
#ifdef ZYPP_ENABLE_ASYNC
      std::string label = (zypp::str::Format(_("Unknown digest %s for file %s.")) %name % file).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptUnknownDigestRequest::makeData(file, name) );
      this->_ctx->sendUserRequest( req );
      return req->choice ();
#else
      return _report->askUserToAccepUnknownDigest( file, name );
#endif
  }


  bool DigestReportHelper::askUserToAcceptWrongDigest(const zypp::Pathname &file, const std::string &requested, const std::string &found)
  {
#ifdef ZYPP_ENABLE_ASYNC
      std::string label = (zypp::str::Format(_("Digest verification failed for file '%s'")) % file).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptWrongDigestRequest::makeData(file, requested, found) );
      this->_ctx->sendUserRequest( req );
      return req->choice ();
#else
      return _report->askUserToAcceptWrongDigest( file, requested, found );
#endif
  }


  bool KeyRingReportHelper::askUserToAcceptUnsignedFile(const std::string &file, const zypp::KeyContext &keycontext)
  {
#ifdef ZYPP_ENABLE_ASYNC
      std::string label;
      if (keycontext.empty())
        label = zypp::str::Format(
              // TranslatorExplanation: speaking of a file
              _("File '%s' is unsigned, continue?")) % file;
      else
        label = zypp::str::Format(
              // TranslatorExplanation: speaking of a file
              _("File '%s' from repository '%s' is unsigned, continue?"))
            % file % keycontext.repoInfo().asUserString();


      auto req = BooleanChoiceRequest::create ( label, false, AcceptUnsignedFileRequest::makeData ( file, keycontext ) );
      this->_ctx->sendUserRequest ( req );
      return req->choice ();
#else
      return _report->askUserToAcceptUnsignedFile( file, keycontext );
#endif
  }


  zypp::KeyRingReport::KeyTrust KeyRingReportHelper::askUserToAcceptKey(const zypp::PublicKey &key, const zypp::KeyContext &keycontext)
  {
#ifdef ZYPP_ENABLE_ASYNC
      auto req = TrustKeyRequest::create(
            _("Do you want to reject the key, trust temporarily, or trust always?"),
            TrustKeyRequest::KEY_DONT_TRUST,
            AcceptKeyRequest::makeData ( key, keycontext )
            );
      this->_ctx->sendUserRequest ( req );
      return static_cast<zypp::KeyRingReport::KeyTrust>(req->choice());
#else
      return _report->askUserToAcceptKey( key, keycontext );
#endif
  }


  bool KeyRingReportHelper::askUserToAcceptPackageKey(const zypp::PublicKey &key_r, const zypp::KeyContext &keycontext_r)
  {
#ifdef ZYPP_ENABLE_ASYNC
      ERR << "Not implemented yet" << std::endl;
      return false;
#else
      return _report->askUserToAcceptPackageKey ( key_r, keycontext_r );
#endif
  }


  void KeyRingReportHelper::infoVerify(const std::string &file_r, const zypp::PublicKeyData &keyData_r, const zypp::KeyContext &keycontext)
  {
#ifdef ZYPP_ENABLE_ASYNC
      std::string label = zypp::str::Format( _("Key Name: %1%")) % keyData_r.name();
      auto req = ShowMessageRequest::create( label, ShowMessageRequest::MType::Info, VerifyInfoEvent::makeData ( file_r, keyData_r, keycontext) );
      this->_ctx->sendUserRequest ( req );
#else
      return _report->infoVerify( file_r, keyData_r, keycontext );
#endif
  }


  void KeyRingReportHelper::reportAutoImportKey(const std::list<zypp::PublicKeyData> &keyDataList_r, const zypp::PublicKeyData &keySigning_r, const zypp::KeyContext &keyContext_r)
  {
#ifdef ZYPP_ENABLE_ASYNC
      const std::string &lbl =  zypp::str::Format( PL_( "Received %1% new package signing key from repository \"%2%\":",
                                                        "Received %1% new package signing keys from repository \"%2%\":",
                                                        keyDataList_r.size() )) % keyDataList_r.size() % keyContext_r.repoInfo().asUserString();
      this->_ctx->sendUserRequest( ShowMessageRequest::create( lbl, ShowMessageRequest::MType::Info, KeyAutoImportInfoEvent::makeData( keyDataList_r, keySigning_r, keyContext_r) ) );
#else
      return _report->reportAutoImportKey( keyDataList_r, keySigning_r, keyContext_r );
#endif
  }


  bool KeyRingReportHelper::askUserToAcceptVerificationFailed(const std::string &file, const zypp::PublicKey &key, const zypp::KeyContext &keycontext)
  {
#ifdef ZYPP_ENABLE_ASYNC
      std::string label;
      if ( keycontext.empty() )
        // translator: %1% is a file name
        label = zypp::str::Format(_("Signature verification failed for file '%1%'.") ) % file;
      else
        // translator: %1% is a file name, %2% a repositories na  me
        label = zypp::str::Format(_("Signature verification failed for file '%1%' from repository '%2%'.") ) % file % keycontext.repoInfo().asUserString();

      // @TODO use a centralized Continue string!
      label += std::string(" ") + _("Continue?");
      auto req = BooleanChoiceRequest::create ( label, false, AcceptFailedVerificationRequest::makeData ( file, key, keycontext ) );
      this->_ctx->sendUserRequest ( req );
      return req->choice ();
#else
      return _report->askUserToAcceptVerificationFailed( file, key, keycontext );
#endif
  }


  bool KeyRingReportHelper::askUserToAcceptUnknownKey( const std::string &file, const std::string &id, const zypp::KeyContext &keycontext )
  {
#ifdef ZYPP_ENABLE_ASYNC
      std::string label;

      if (keycontext.empty())
        label = zypp::str::Format(
              // translators: the last %s is gpg key ID
              _("File '%s' is signed with an unknown key '%s'. Continue?")) % file % id;
      else
        label = zypp::str::Format(
              // translators: the last %s is gpg key ID
              _("File '%s' from repository '%s' is signed with an unknown key '%s'. Continue?"))
            % file % keycontext.repoInfo().asUserString() % id;

      auto req = BooleanChoiceRequest::create ( label, false, AcceptUnknownKeyRequest::makeData ( file, id, keycontext ) );
      this->_ctx->sendUserRequest ( req );
      return req->choice ();
#else
      return _report->askUserToAcceptUnknownKey( file, id, keycontext );
#endif
  }


  bool JobReportHelper::debug(std::string msg_r, UserData userData_r)
  {
#ifdef ZYPP_ENABLE_ASYNC
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Debug, std::move(userData_r) ) );
      return true;
#else
      return zypp::JobReport::debug ( msg_r, userData_r );
#endif
  }


  bool JobReportHelper::info( std::string msg_r, UserData userData_r)
  {
#ifdef ZYPP_ENABLE_ASYNC
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Info, std::move(userData_r) ) );
      return true;
#else
      return zypp::JobReport::info ( msg_r, userData_r );
#endif
  }


  bool JobReportHelper::warning( std::string msg_r, UserData userData_r)
  {
#ifdef ZYPP_ENABLE_ASYNC
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Warning, std::move(userData_r) ) );
      return true;
#else
      return zypp::JobReport::warning ( msg_r, userData_r );
#endif
  }


  bool JobReportHelper::error( std::string msg_r, UserData userData_r)
  {
#ifdef ZYPP_ENABLE_ASYNC
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Error, std::move(userData_r) ) );
      return true;
#else
      return zypp::JobReport::error ( msg_r, userData_r );
#endif
  }


  bool JobReportHelper::important( std::string msg_r, UserData userData_r)
  {
#ifdef ZYPP_ENABLE_ASYNC
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Important, std::move(userData_r) ) );
      return true;
#else
      return zypp::JobReport::important ( msg_r, userData_r );
#endif
  }


  bool JobReportHelper::data( std::string msg_r, UserData userData_r)
  {
#ifdef ZYPP_ENABLE_ASYNC
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Data, std::move(userData_r) ) );
      return true;
#else
      return zypp::JobReport::data ( msg_r, userData_r );
#endif
  }
}
