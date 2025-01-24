/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/zypp_detail/repomanagerbase_p.h
 *
*/
#ifndef ZYPP_NG_REPORTHELPER_INCLUDED
#define ZYPP_NG_REPORTHELPER_INCLUDED

#include <zypp/PublicKey.h>
#include <zypp/KeyContext.h>
#include <zypp/KeyRing.h>
#include <zypp/Digest.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/zyppng/base/zyppglobal.h>
#include <zypp-core/zyppng/ui/progressobserver.h>


#include <zypp/ng/Context>
#include <zypp/ng/userrequest.h>
#include <zypp/ng/repoinfo.h>
#include <zypp/ng/workflows/logichelpers.h>

#include <zypp/ZYppCallbacks.h>

namespace zyppng {

  struct NewStyleReportTag {};
  struct LegacyStyleReportTag{};

  namespace detail {

    template <typename T>
    struct DefaultReportTag;

    template<>
    struct DefaultReportTag<AsyncContextRef> {
      using Type = NewStyleReportTag;
    };

    template<>
    struct DefaultReportTag<SyncContextRef> {
      using Type = LegacyStyleReportTag;
    };

    template <typename T>
    using default_report_tag_t = NewStyleReportTag; // typename DefaultReportTag<T>::Type;

    template <typename ContextRefType, typename Report>
    class ReportHolder;

    template <typename Report>
    class ReportHolder<AsyncContextRef, Report>
    {};

    template <typename Report>
    class ReportHolder<SyncContextRef, Report>
    {
    public:
      ReportHolder() : _d( std::make_shared<zypp::callback::SendReport<Report>>() ){}
      virtual ~ReportHolder(){};

      ReportHolder(const ReportHolder &) = default;
      ReportHolder(ReportHolder &&) = default;
      ReportHolder &operator=(const ReportHolder &) = default;
      ReportHolder &operator=(ReportHolder &&) = default;

      auto &operator->() {
        return *_d;
      }

      std::shared_ptr<zypp::callback::SendReport<Report>> _d;
    };
  }

  template<typename ZyppContextRef, typename ReportTag >
  class BasicReportHelper : public MaybeAsyncMixin< std::is_same_v< typename remove_smart_ptr_t<ZyppContextRef>::SyncOrAsyncTag, AsyncTag> > {
  public:
    virtual ~BasicReportHelper() = default;
    BasicReportHelper(const BasicReportHelper &) = default;
    BasicReportHelper(BasicReportHelper &&) = default;
    BasicReportHelper &operator=(const BasicReportHelper &) = default;
    BasicReportHelper &operator=(BasicReportHelper &&) = default;

    const ZyppContextRef &zyppContext() {
      return _ctx;
    }

    static constexpr bool useNewStyleReports () {
      return std::is_same_v<ReportTag, NewStyleReportTag>;
    }

  protected:
    BasicReportHelper( ZyppContextRef &&ctx, ProgressObserverRef &&taskObserver )
      : _ctx(std::move(ctx))
      , _taskObserver( std::move(taskObserver) )
    { }
    ZyppContextRef _ctx;
    ProgressObserverRef _taskObserver;
  };

  template<typename ZyppContextRef, typename ReportTag = detail::default_report_tag_t<ZyppContextRef>>
  class DigestReportHelper : public BasicReportHelper<ZyppContextRef, ReportTag>  {
  public:
    using BasicReportHelper<ZyppContextRef, ReportTag>::useNewStyleReports;

    DigestReportHelper(ZyppContextRef r, ProgressObserverRef taskObserver)
      : BasicReportHelper<ZyppContextRef, ReportTag>(std::move(r), std::move(taskObserver)) {}

    virtual ~DigestReportHelper() = default;

    DigestReportHelper(const DigestReportHelper &) = default;
    DigestReportHelper(DigestReportHelper &&) = default;
    DigestReportHelper &operator=(const DigestReportHelper &) = default;
    DigestReportHelper &operator=(DigestReportHelper &&) = default;

    bool askUserToAcceptNoDigest ( const zypp::Pathname &file )
    {
      if constexpr ( useNewStyleReports() ) {
        std::string label = (zypp::str::Format(_("No digest for file %s.")) % file ).str();
        auto req = BooleanChoiceRequest::create( label, false, AcceptNoDigestRequest::makeData(file) );
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest( req );
        return req->choice ();
      } else {
        return _report->askUserToAcceptNoDigest(file);
      }
    }
    bool askUserToAccepUnknownDigest ( const zypp::Pathname &file, const std::string &name )
    {
      if constexpr ( useNewStyleReports() ) {
        std::string label = (zypp::str::Format(_("Unknown digest %s for file %s.")) %name % file).str();
        auto req = BooleanChoiceRequest::create( label, false, AcceptUnknownDigestRequest::makeData(file, name) );
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest( req );
        return req->choice ();
      } else {
        return _report->askUserToAccepUnknownDigest( file, name );
      }
    }
    bool askUserToAcceptWrongDigest ( const zypp::Pathname &file, const std::string &requested, const std::string &found )
    {
      if constexpr ( useNewStyleReports() ) {
        std::string label = (zypp::str::Format(_("Digest verification failed for file '%s'")) % file).str();
        auto req = BooleanChoiceRequest::create( label, false, AcceptWrongDigestRequest::makeData(file, requested, found) );
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest( req );
        return req->choice ();
      } else {
        return _report->askUserToAcceptWrongDigest( file, requested, found );
      }
    }

  private:
    detail::ReportHolder<ZyppContextRef, zypp::DigestReport> _report;
  };

  template<typename ZyppContextRef, typename ReportTag = detail::default_report_tag_t<ZyppContextRef>>
  class KeyRingReportHelper : public BasicReportHelper<ZyppContextRef, ReportTag>  {
  public:
    using BasicReportHelper<ZyppContextRef, ReportTag>::useNewStyleReports;

    KeyRingReportHelper(ZyppContextRef r, ProgressObserverRef taskObserver)
      : BasicReportHelper<ZyppContextRef, ReportTag>(std::move(r), std::move(taskObserver)) {}
    virtual ~KeyRingReportHelper() = default;

    KeyRingReportHelper(const KeyRingReportHelper &) = default;
    KeyRingReportHelper(KeyRingReportHelper &&) = default;
    KeyRingReportHelper &operator=(const KeyRingReportHelper &) = default;
    KeyRingReportHelper &operator=(KeyRingReportHelper &&) = default;

    // -- Key Ring reports -- //

    bool askUserToAcceptUnsignedFile( const std::string &file, const zypp::KeyContext &keycontext = {} )
    {
      if constexpr ( useNewStyleReports() ) {
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
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest ( req );
        return req->choice ();
      } else {
        return _report->askUserToAcceptUnsignedFile( file, keycontext );
      }
    }

    zypp::KeyRingReport::KeyTrust askUserToAcceptKey( const zypp::PublicKey &key, const zypp::KeyContext &keycontext = {} )
    {
      if constexpr ( useNewStyleReports() ) {


        auto req = ListChoiceRequest::create(
              _("Do you want to reject the key, trust temporarily, or trust always?"),
              std::vector<ListChoiceRequest::Choice>{
                { _("r"), _("Reject.") },
                { _("t"), _("Trust temporarily.") },
                { _("a"), _("Trust always.") }
              },
              0, // default is option 0, do not trust
              AcceptKeyRequest::makeData ( key, keycontext )
              );
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest ( req );
        switch (req->choice()) {
          default:
          case 0:
            return zypp::KeyRingReport::KeyTrust::KEY_DONT_TRUST;
          case 1:
            return zypp::KeyRingReport::KeyTrust::KEY_TRUST_TEMPORARILY;
          case 2:
            return zypp::KeyRingReport::KEY_TRUST_AND_IMPORT;

        };
      } else {
        return _report->askUserToAcceptKey( key, keycontext );
      }
    }

    bool askUserToAcceptPackageKey( const zypp::PublicKey &key_r, const zypp::KeyContext &keycontext_r = {} )
    {
      if constexpr ( useNewStyleReports() ) {
        ERR << "Not implemented yet" << std::endl;
        return false;
      } else {
        return _report->askUserToAcceptPackageKey ( key_r, keycontext_r );
      }
    }

    void infoVerify( const std::string & file_r, const zypp::PublicKeyData & keyData_r, const zypp::KeyContext &keycontext = {} )
    {
      if constexpr ( useNewStyleReports() ) {
        std::string label = zypp::str::Format( _("Key Name: %1%")) % keyData_r.name();
        auto req = ShowMessageRequest::create( label, ShowMessageRequest::MType::Info, VerifyInfoEvent::makeData ( file_r, keyData_r, keycontext) );
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest ( req );
      } else {
        return _report->infoVerify( file_r, keyData_r, keycontext );
      }
    }

    void reportAutoImportKey( const std::list<zypp::PublicKeyData> & keyDataList_r, const zypp::PublicKeyData & keySigning_r, const zypp::KeyContext &keyContext_r )
    {
      if constexpr ( useNewStyleReports() ) {

        const auto &repoInfoOpt = keyContext_r.ngRepoInfo();

        const std::string &lbl =  zypp::str::Format( PL_( "Received %1% new package signing key from repository \"%2%\":",
                                                          "Received %1% new package signing keys from repository \"%2%\":",
                                                          keyDataList_r.size() )) % keyDataList_r.size() % ( repoInfoOpt ? repoInfoOpt->asUserString() : std::string("norepo") );
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest( ShowMessageRequest::create( lbl, ShowMessageRequest::MType::Info, KeyAutoImportInfoEvent::makeData( keyDataList_r, keySigning_r, keyContext_r) ) );
      } else {
        return _report->reportAutoImportKey( keyDataList_r, keySigning_r, keyContext_r );
      }
    }

    bool askUserToAcceptVerificationFailed( const std::string &file, const zypp::PublicKey &key, const zypp::KeyContext &keycontext = {} )
    {
      if constexpr ( useNewStyleReports() ) {
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
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest ( req );
        return req->choice ();
      } else {
        return _report->askUserToAcceptVerificationFailed( file, key, keycontext );
      }
    }

    bool askUserToAcceptUnknownKey( const std::string &file, const std::string &id, const zypp::KeyContext &keycontext = {} )
    {
      if constexpr ( useNewStyleReports() ) {
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
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest ( req );
        return req->choice ();
      } else {
        return _report->askUserToAcceptUnknownKey( file, id, keycontext );
      }
    }

  private:
    detail::ReportHolder<ZyppContextRef, zypp::KeyRingReport> _report;

  };

  template<typename ZyppContextRef, typename ReportTag = detail::default_report_tag_t<ZyppContextRef>>
  class JobReportHelper : public BasicReportHelper<ZyppContextRef, ReportTag>  {
  public:
    using BasicReportHelper<ZyppContextRef, ReportTag>::useNewStyleReports;

    JobReportHelper(ZyppContextRef r, ProgressObserverRef taskObserver)
      : BasicReportHelper<ZyppContextRef, ReportTag>(std::move(r), std::move(taskObserver)) {}
    virtual ~JobReportHelper() = default;

    JobReportHelper(const JobReportHelper &) = default;
    JobReportHelper(JobReportHelper &&) = default;
    JobReportHelper &operator=(const JobReportHelper &) = default;
    JobReportHelper &operator=(JobReportHelper &&) = default;

    /** send debug message text */
    bool debug( std::string msg_r, UserData userData_r = UserData() )
    {
      if constexpr ( useNewStyleReports() ) {
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Debug, std::move(userData_r) ) );
        return true;
      } else {
        return zypp::JobReport::debug ( msg_r, userData_r );
      }
    }

    /** send message text */
    bool info( std::string msg_r, UserData userData_r = UserData() )
    {
      if constexpr ( useNewStyleReports() ) {
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Info, std::move(userData_r) ) );
        return true;
      } else {
        return zypp::JobReport::info ( msg_r, userData_r );
      }
    }

    /** send warning text */
    bool warning( std::string msg_r, UserData userData_r = UserData() )
    {
      if constexpr ( useNewStyleReports() ) {
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Warning, std::move(userData_r) ) );
        return true;
      } else {
        return zypp::JobReport::warning ( msg_r, userData_r );
      }
    }

    /** send error text */
    bool error( std::string msg_r, UserData userData_r = UserData() )
    {
      if constexpr ( useNewStyleReports() ) {
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Error, std::move(userData_r) ) );
        return true;
      } else {
        return zypp::JobReport::error ( msg_r, userData_r );
      }
    }

    /** send important message text */
    bool important( std::string msg_r, UserData userData_r = UserData() )
    {
      if constexpr ( useNewStyleReports() ) {
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Important, std::move(userData_r) ) );
        return true;
      } else {
        return zypp::JobReport::important ( msg_r, userData_r );
      }
    }

    /** send data message */
    bool data( std::string msg_r, UserData userData_r = UserData() )
    {
      if constexpr ( useNewStyleReports() ) {
        if ( this->_taskObserver ) this->_taskObserver->sendUserRequest( ShowMessageRequest::create( std::move(msg_r), ShowMessageRequest::MType::Data, std::move(userData_r) ) );
        return true;
      } else {
        return zypp::JobReport::data ( msg_r, userData_r );
      }
    }

  };
}


#endif //ZYPP_NG_REPORTHELPER_INCLUDED
