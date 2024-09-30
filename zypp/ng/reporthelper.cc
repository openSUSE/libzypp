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
#include <zypp/ng/Context>
#include <zypp/ng/userrequest.h>
#include <zypp/ng/repoinfo.h>

namespace zyppng {

  template<typename ZyppContextRef>
  BasicReportHelper<ZyppContextRef>::BasicReportHelper(ZyppContextRef &&ctx)
    : _ctx( std::move(ctx) )
  { }

  template<typename ZyppContextRef>
  bool DigestReportHelper<ZyppContextRef>::askUserToAcceptNoDigest(const zypp::Pathname &file)
  {
    if constexpr ( async() ) {
      std::string label = (zypp::str::Format(_("No digest for file %s.")) % file ).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptNoDigestRequest::makeData(file) );
      this->_ctx->sendUserRequest( req );
      return req->choice ();
    } else {
      return _report->askUserToAcceptNoDigest(file);
    }
  }

  template<typename ZyppContextRef>
  bool DigestReportHelper<ZyppContextRef>::askUserToAccepUnknownDigest(const zypp::Pathname &file, const std::string &name)
  {
    if constexpr ( async() ) {
      std::string label = (zypp::str::Format(_("Unknown digest %s for file %s.")) %name % file).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptUnknownDigestRequest::makeData(file, name) );
      this->_ctx->sendUserRequest( req );
      return req->choice ();
    } else {
      return _report->askUserToAccepUnknownDigest( file, name );
    }
  }

  template<typename ZyppContextRef>
  bool DigestReportHelper<ZyppContextRef>::askUserToAcceptWrongDigest(const zypp::Pathname &file, const std::string &requested, const std::string &found)
  {
    if constexpr ( async() ) {
      std::string label = (zypp::str::Format(_("Digest verification failed for file '%s'")) % file).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptWrongDigestRequest::makeData(file, requested, found) );
      this->_ctx->sendUserRequest( req );
      return req->choice ();
    } else {
      return _report->askUserToAcceptWrongDigest( file, requested, found );
    }
  }

  template<typename ZyppContextRef>
  bool KeyRingReportHelper<ZyppContextRef>::askUserToAcceptUnsignedFile(const std::string &file, const zypp::KeyContext &keycontext)
  {
    if constexpr ( async() ) {
      std::string label;
      if (keycontext.empty())
        label = zypp::str::Format(
              // TranslatorExplanation: speaking of a file
              _("File '%s' is unsigned, continue?")) % file;
      else
        label = zypp::str::Format(
              // TranslatorExplanation: speaking of a file
              _("File '%s' from repository '%s' is unsigned, continue?"))
            % file % keycontext.ngRepoInfo()->asUserString();


      auto req = BooleanChoiceRequest::create ( label, false, AcceptUnsignedFileRequest::makeData ( file, keycontext ) );
      this->_ctx->sendUserRequest ( req );
      return req->choice ();
    } else {
      return _report->askUserToAcceptUnsignedFile( file, keycontext );
    }
  }

  template<typename ZyppContextRef>
  zypp::KeyRingReport::KeyTrust KeyRingReportHelper<ZyppContextRef>::askUserToAcceptKey(const zypp::PublicKey &key, const zypp::KeyContext &keycontext)
  {
    if constexpr ( async() ) {
      auto req = TrustKeyRequest::create(
            _("Do you want to reject the key, trust temporarily, or trust always?"),
            TrustKeyRequest::KEY_DONT_TRUST,
            AcceptKeyRequest::makeData ( key, keycontext )
            );
      this->_ctx->sendUserRequest ( req );
      return static_cast<zypp::KeyRingReport::KeyTrust>(req->choice());
    } else {
      return _report->askUserToAcceptKey( key, keycontext );
    }
  }

  template<typename ZyppContextRef>
  bool KeyRingReportHelper<ZyppContextRef>::askUserToAcceptPackageKey(const zypp::PublicKey &key_r, const zypp::KeyContext &keycontext_r)
  {
    if constexpr ( async() ) {
      ERR << "Not implemented yet" << std::endl;
      return false;
    } else {
      return _report->askUserToAcceptPackageKey ( key_r, keycontext_r );
    }
  }

  template<typename ZyppContextRef>
  void KeyRingReportHelper<ZyppContextRef>::infoVerify(const std::string &file_r, const zypp::PublicKeyData &keyData_r, const zypp::KeyContext &keycontext)
  {
    if constexpr ( async() ) {
      std::string label = zypp::str::Format( _("Key Name: %1%")) % keyData_r.name();
      auto req = ShowMessageRequest::create( label, ShowMessageRequest::MType::Info, VerifyInfoEvent::makeData ( file_r, keyData_r, keycontext) );
      this->_ctx->sendUserRequest ( req );
    } else {
      return _report->infoVerify( file_r, keyData_r, keycontext );
    }
  }

  template<typename ZyppContextRef>
  void KeyRingReportHelper<ZyppContextRef>::reportAutoImportKey(const std::list<zypp::PublicKeyData> &keyDataList_r, const zypp::PublicKeyData &keySigning_r, const zypp::KeyContext &keyContext_r)
  {
    if constexpr ( async() ) {

      const auto &repoInfoOpt = keyContext_r.ngRepoInfo();

      const std::string &lbl =  zypp::str::Format( PL_( "Received %1% new package signing key from repository \"%2%\":",
                                                        "Received %1% new package signing keys from repository \"%2%\":",
                                                        keyDataList_r.size() )) % keyDataList_r.size() % ( repoInfoOpt ? repoInfoOpt->asUserString() : std::string("norepo") );
      this->_ctx->sendUserRequest( ShowMessageRequest::create( lbl, ShowMessageRequest::MType::Info, KeyAutoImportInfoEvent::makeData( keyDataList_r, keySigning_r, keyContext_r) ) );
    } else {
      return _report->reportAutoImportKey( keyDataList_r, keySigning_r, keyContext_r );
    }
  }

  template<typename ZyppContextRef>
  bool KeyRingReportHelper<ZyppContextRef>::askUserToAcceptVerificationFailed(const std::string &file, const zypp::PublicKey &key, const zypp::KeyContext &keycontext)
  {
    if constexpr ( async() ) {
      std::string label;
      if ( keycontext.empty() )
        // translator: %1% is a file name
        label = zypp::str::Format(_("Signature verification failed for file '%1%'.") ) % file;
      else
        // translator: %1% is a file name, %2% a repositories na  me
        label = zypp::str::Format(_("Signature verification failed for file '%1%' from repository '%2%'.") ) % file % keycontext.ngRepoInfo()->asUserString();

      // @TODO use a centralized Continue string!
      label += std::string(" ") + _("Continue?");
      auto req = BooleanChoiceRequest::create ( label, false, AcceptFailedVerificationRequest::makeData ( file, key, keycontext ) );
      this->_ctx->sendUserRequest ( req );
      return req->choice ();
    } else {
      return _report->askUserToAcceptVerificationFailed( file, key, keycontext );
    }
  }

  template<typename ZyppContextRef>
  bool KeyRingReportHelper<ZyppContextRef>::askUserToAcceptUnknownKey( const std::string &file, const std::string &id, const zypp::KeyContext &keycontext )
  {
    if constexpr ( async() ) {
      std::string label;

      if (keycontext.empty())
        label = zypp::str::Format(
              // translators: the last %s is gpg key ID
              _("File '%s' is signed with an unknown key '%s'. Continue?")) % file % id;
      else
        label = zypp::str::Format(
              // translators: the last %s is gpg key ID
              _("File '%s' from repository '%s' is signed with an unknown key '%s'. Continue?"))
            % file % keycontext.ngRepoInfo()->asUserString() % id;

      auto req = BooleanChoiceRequest::create ( label, false, AcceptUnknownKeyRequest::makeData ( file, id, keycontext ) );
      this->_ctx->sendUserRequest ( req );
      return req->choice ();
    } else {
      return _report->askUserToAcceptUnknownKey( file, id, keycontext );
    }
  }

  template<typename ZyppContextRef>
  bool JobReportHelper<ZyppContextRef>::debug(std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Debug, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::debug ( msg_r, userData_r );
    }
  }

  template<typename ZyppContextRef>
  bool JobReportHelper<ZyppContextRef>::info( std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Info, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::info ( msg_r, userData_r );
    }
  }

  template<typename ZyppContextRef>
  bool JobReportHelper<ZyppContextRef>::warning( std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Warning, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::warning ( msg_r, userData_r );
    }
  }

  template<typename ZyppContextRef>
  bool JobReportHelper<ZyppContextRef>::error( std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Error, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::error ( msg_r, userData_r );
    }
  }

  template<typename ZyppContextRef>
  bool JobReportHelper<ZyppContextRef>::important( std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Important, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::important ( msg_r, userData_r );
    }
  }

  template<typename ZyppContextRef>
  bool JobReportHelper<ZyppContextRef>::data( std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      this->_ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Data, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::data ( msg_r, userData_r );
    }
  }

  // explicitely intantiate the template types we want to work with
  template class BasicReportHelper<SyncContextRef>;
  template class BasicReportHelper<AsyncContextRef>;
  template class DigestReportHelper<SyncContextRef>;
  template class DigestReportHelper<AsyncContextRef>;
  template class KeyRingReportHelper<SyncContextRef>;
  template class KeyRingReportHelper<AsyncContextRef>;
  template class JobReportHelper<SyncContextRef>;
  template class JobReportHelper<AsyncContextRef>;


}
