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

#include <zypp-common/PublicKey.h>
#include <zypp/KeyContext.h>
#include <zypp/KeyRing.h>
#include <zypp/Digest.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/ng/base/zyppglobal.h>
#include <zypp-core/ng/ui/userrequest.h>
#include <zypp/ng/context.h>

#include <zypp/ZYppCallbacks.h>

namespace zyppng {

  ZYPP_FWD_DECL_REFS (Context);

  namespace detail {
    template <typename Report, bool syncAsync >
    class ReportHolder;

    template <typename Report>
    class ReportHolder<Report, true>
    {};

    template <typename Report>
    class ReportHolder<Report, false>
    {
    public:
      ReportHolder() : _d( std::make_shared<zypp::callback::SendReport<Report>>() ){}

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

  class BasicReportHelper {
  public:
    BasicReportHelper(const BasicReportHelper &) = default;
    BasicReportHelper(BasicReportHelper &&) = default;
    BasicReportHelper &operator=(const BasicReportHelper &) = default;
    BasicReportHelper &operator=(BasicReportHelper &&) = default;

    const ContextRef &zyppContext() {
      return _ctx;
    }

    static constexpr bool async () {
      return ZYPP_IS_ASYNC;
    }

  protected:
    BasicReportHelper( ContextRef &&ctx );
    ContextRef _ctx;
  };

  class DigestReportHelper : public BasicReportHelper  {
  public:
    using BasicReportHelper::async;

    DigestReportHelper(ContextRef r)
      : BasicReportHelper(std::move(r)) {}

    DigestReportHelper(const DigestReportHelper &) = default;
    DigestReportHelper(DigestReportHelper &&) = default;
    DigestReportHelper &operator=(const DigestReportHelper &) = default;
    DigestReportHelper &operator=(DigestReportHelper &&) = default;

    bool askUserToAcceptNoDigest ( const zypp::Pathname &file );
    bool askUserToAccepUnknownDigest ( const zypp::Pathname &file, const std::string &name );
    bool askUserToAcceptWrongDigest ( const zypp::Pathname &file, const std::string &requested, const std::string &found );

  private:
    detail::ReportHolder<zypp::DigestReport, Context::isAsync> _report;
  };

  class KeyRingReportHelper : public BasicReportHelper  {
  public:
    using BasicReportHelper::async;

    KeyRingReportHelper(ContextRef r)
      : BasicReportHelper(std::move(r)) {}

    KeyRingReportHelper(const KeyRingReportHelper &) = default;
    KeyRingReportHelper(KeyRingReportHelper &&) = default;
    KeyRingReportHelper &operator=(const KeyRingReportHelper &) = default;
    KeyRingReportHelper &operator=(KeyRingReportHelper &&) = default;

    // -- Key Ring reports -- //

    bool askUserToAcceptUnsignedFile( const std::string &file, const zypp::KeyContext &keycontext = {} );

    zypp::KeyRingReport::KeyTrust askUserToAcceptKey( const zypp::PublicKey &key, const zypp::KeyContext &keycontext = {} );

    bool askUserToAcceptPackageKey( const zypp::PublicKey &key_r, const zypp::KeyContext &keycontext_r = {} );

    void infoVerify( const std::string & file_r, const zypp::PublicKeyData & keyData_r, const zypp::KeyContext &keycontext = {} );

    void reportAutoImportKey( const std::list<zypp::PublicKeyData> & keyDataList_r, const zypp::PublicKeyData & keySigning_r, const zypp::KeyContext &keyContext_r );

    bool askUserToAcceptVerificationFailed( const std::string &file, const zypp::PublicKey &key, const zypp::KeyContext &keycontext = {} );

    bool askUserToAcceptUnknownKey( const std::string &file, const std::string &id, const zypp::KeyContext &keycontext = {} );

  private:
    detail::ReportHolder<zypp::KeyRingReport, Context::isAsync> _report;

  };

  class JobReportHelper : public BasicReportHelper  {
  public:
    using BasicReportHelper::async;

    JobReportHelper(ContextRef r)
      : BasicReportHelper(std::move(r)) {}

    JobReportHelper(const JobReportHelper &) = default;
    JobReportHelper(JobReportHelper &&) = default;
    JobReportHelper &operator=(const JobReportHelper &) = default;
    JobReportHelper &operator=(JobReportHelper &&) = default;

    /** send debug message text */
    bool debug( std::string msg_r, UserData userData_r = UserData() );

    /** send message text */
    bool info( std::string msg_r, UserData userData_r = UserData() );

    /** send warning text */
    bool warning( std::string msg_r, UserData userData_r = UserData() );

    /** send error text */
    bool error( std::string msg_r, UserData userData_r = UserData() );

    /** send important message text */
    bool important( std::string msg_r, UserData userData_r = UserData() );

    /** send data message */
    bool data( std::string msg_r, UserData userData_r = UserData() );

  };
}


#endif //ZYPP_NG_REPORTHELPER_INCLUDED
