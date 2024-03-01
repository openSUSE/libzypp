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
#include <zypp/ZYppCallbacks.h>

namespace zyppng {

  template<typename ZyppContextRef>
  ReportHelper<ZyppContextRef>::ReportHelper(ZyppContextRef ctx)
    : _ctx( std::move(ctx) )
  { }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::askUserToAcceptNoDigest(const zypp::Pathname &file)
  {
    if constexpr ( async() ) {
      std::string label = (zypp::str::Format(_("No digest for file %s.")) % file ).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptNoDigestRequest::makeData(file) );
      _ctx->sendUserRequest( req );
      return req->choice ();
    } else {
      zypp::callback::SendReport<zypp::DigestReport> report;
      return report->askUserToAcceptNoDigest(file);
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::askUserToAccepUnknownDigest(const zypp::Pathname &file, const std::string &name)
  {
    if constexpr ( async() ) {
      std::string label = (zypp::str::Format(_("Unknown digest %s for file %s.")) %name % file).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptUnknownDigestRequest::makeData(file, name) );
      _ctx->sendUserRequest( req );
      return req->choice ();
    } else {
      zypp::callback::SendReport<zypp::DigestReport> report;
      return report->askUserToAccepUnknownDigest( file, name );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::askUserToAcceptWrongDigest(const zypp::Pathname &file, const std::string &requested, const std::string &found)
  {
    if constexpr ( async() ) {
      std::string label = (zypp::str::Format(_("Digest verification failed for file '%s'")) % file).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptWrongDigestRequest::makeData(file, requested, found) );
      _ctx->sendUserRequest( req );
      return req->choice ();
    } else {
      zypp::callback::SendReport<zypp::DigestReport> report;
      return report->askUserToAcceptWrongDigest( file, requested, found );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::askUserToAcceptUnsignedFile(const std::string &file, const zypp::KeyContext &keycontext)
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
            % file % keycontext.repoInfo().asUserString();


      auto req = BooleanChoiceRequest::create ( label, false, AcceptUnsignedFileRequest::makeData ( file, keycontext ) );
      _ctx->sendUserRequest ( req );
      return req->choice ();
    } else {
      zypp::callback::SendReport<zypp::KeyRingReport> report;
      return report->askUserToAcceptUnsignedFile( file, keycontext );
    }
  }

  template<typename ZyppContextRef>
  zypp::KeyRingReport::KeyTrust ReportHelper<ZyppContextRef>::askUserToAcceptKey(const zypp::PublicKey &key, const zypp::KeyContext &keycontext)
  {
    if constexpr ( async() ) {
      auto req = TrustKeyRequest::create(
            _("Do you want to reject the key, trust temporarily, or trust always?"),
            TrustKeyRequest::KEY_DONT_TRUST,
            AcceptKeyRequest::makeData ( key, keycontext )
            );
      _ctx->sendUserRequest ( req );
      return static_cast<zypp::KeyRingReport::KeyTrust>(req->choice());
    } else {
      zypp::callback::SendReport<zypp::KeyRingReport> report;
      return report->askUserToAcceptKey( key, keycontext );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::askUserToAcceptPackageKey(const zypp::PublicKey &key_r, const zypp::KeyContext &keycontext_r)
  {
    if constexpr ( async() ) {
      ERR << "Not implemented yet" << std::endl;
      return false;
    } else {
      zypp::callback::SendReport<zypp::KeyRingReport> report;
      return report->askUserToAcceptPackageKey ( key_r, keycontext_r );
    }
  }

  template<typename ZyppContextRef>
  void ReportHelper<ZyppContextRef>::infoVerify(const std::string &file_r, const zypp::PublicKeyData &keyData_r, const zypp::KeyContext &keycontext)
  {
    if constexpr ( async() ) {
      std::string label = zypp::str::Format( _("Key Name: %1%")) % keyData_r.name();
      auto req = ShowMessageRequest::create( label, ShowMessageRequest::MType::Info, VerifyInfoEvent::makeData ( file_r, keyData_r, keycontext) );
      _ctx->sendUserRequest ( req );
    } else {
      zypp::callback::SendReport<zypp::KeyRingReport> report;
      return report->infoVerify( file_r, keyData_r, keycontext );
    }
  }

  template<typename ZyppContextRef>
  void ReportHelper<ZyppContextRef>::reportAutoImportKey(const std::list<zypp::PublicKeyData> &keyDataList_r, const zypp::PublicKeyData &keySigning_r, const zypp::KeyContext &keyContext_r)
  {
    if constexpr ( async() ) {
      const std::string &lbl =  zypp::str::Format( PL_( "Received %1% new package signing key from repository \"%2%\":",
                                                        "Received %1% new package signing keys from repository \"%2%\":",
                                                        keyDataList_r.size() )) % keyDataList_r.size() % keyContext_r.repoInfo().asUserString();
      _ctx->sendUserRequest( ShowMessageRequest::create( lbl, ShowMessageRequest::MType::Info, KeyAutoImportInfoEvent::makeData( keyDataList_r, keySigning_r, keyContext_r) ) );
    } else {
      zypp::callback::SendReport<zypp::KeyRingReport> report;
      return report->reportAutoImportKey( keyDataList_r, keySigning_r, keyContext_r );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::askUserToAcceptVerificationFailed(const std::string &file, const zypp::PublicKey &key, const zypp::KeyContext &keycontext)
  {
    if constexpr ( async() ) {
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
      _ctx->sendUserRequest ( req );
      return req->choice ();
    } else {
      zypp::callback::SendReport<zypp::KeyRingReport> report;
      return report->askUserToAcceptVerificationFailed( file, key, keycontext );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::askUserToAcceptUnknownKey( const std::string &file, const std::string &id, const zypp::KeyContext &keycontext )
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
            % file % keycontext.repoInfo().asUserString() % id;

      auto req = BooleanChoiceRequest::create ( label, false, AcceptUnknownKeyRequest::makeData ( file, id, keycontext ) );
      _ctx->sendUserRequest ( req );
      return req->choice ();
    } else {
      zypp::callback::SendReport<zypp::KeyRingReport> report;
      return report->askUserToAcceptUnknownKey( file, id, keycontext );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::debug(std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      _ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Debug, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::debug ( msg_r, userData_r );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::info( std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      _ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Info, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::info ( msg_r, userData_r );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::warning( std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      _ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Warning, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::warning ( msg_r, userData_r );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::error( std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      _ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Error, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::error ( msg_r, userData_r );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::important( std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      _ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Important, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::important ( msg_r, userData_r );
    }
  }

  template<typename ZyppContextRef>
  bool ReportHelper<ZyppContextRef>::data( std::string msg_r, UserData userData_r)
  {
    if constexpr ( async() ) {
      _ctx->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Data, std::move(userData_r) ) );
      return true;
    } else {
      return zypp::JobReport::data ( msg_r, userData_r );
    }
  }

  // explicitely intantiate the template types we want to work with
  template class ReportHelper<SyncContextRef>;
  template class ReportHelper<ContextRef>;
}
