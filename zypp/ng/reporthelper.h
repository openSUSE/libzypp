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

#include <memory>

#include <zypp/PublicKey.h>
#include <zypp/KeyContext.h>
#include <zypp/KeyRing.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/zyppng/base/zyppglobal.h>
#include <zypp-core/zyppng/ui/userrequest.h>
#include <zypp/ng/context.h>
#include <zypp/ng/workflows/contextfacade.h>

namespace zyppng {

  ZYPP_FWD_DECL_REFS (Context);

  template<typename ZyppContextRef>
  class ReportHelper {
  public:
    ReportHelper(const ReportHelper &) = default;
    ReportHelper(ReportHelper &&) = default;
    ReportHelper &operator=(const ReportHelper &) = default;
    ReportHelper &operator=(ReportHelper &&) = default;

    ReportHelper( ZyppContextRef ctx );

    // -- Digest reports -- //

    bool askUserToAcceptNoDigest ( const zypp::Pathname &file );
    bool askUserToAccepUnknownDigest ( const zypp::Pathname &file, const std::string &name );
    bool askUserToAcceptWrongDigest ( const zypp::Pathname &file, const std::string &requested, const std::string &found );

    // -- Key Ring reports -- //

    bool askUserToAcceptUnsignedFile( const std::string &file, const zypp::KeyContext &keycontext = {} );

    zypp::KeyRingReport::KeyTrust askUserToAcceptKey( const zypp::PublicKey &key, const zypp::KeyContext &keycontext = {} );

    bool askUserToAcceptPackageKey( const zypp::PublicKey &key_r, const zypp::KeyContext &keycontext_r = {} );

    void infoVerify( const std::string & file_r, const zypp::PublicKeyData & keyData_r, const zypp::KeyContext &keycontext = {} );

    void reportAutoImportKey( const std::list<zypp::PublicKeyData> & keyDataList_r, const zypp::PublicKeyData & keySigning_r, const zypp::KeyContext &keyContext_r );

    bool askUserToAcceptVerificationFailed( const std::string &file, const zypp::PublicKey &key, const zypp::KeyContext &keycontext = {} );

    bool askUserToAcceptUnknownKey( const std::string &file, const std::string &id, const zypp::KeyContext &keycontext = {} );

    // -- Job Reports -- //

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

    const ZyppContextRef &zyppContext() {
      return _ctx;
    }

  private:

    static constexpr bool async () {
      return std::is_same<ZyppContextRef, ContextRef>();
    }

    ZyppContextRef _ctx;
  };



}


#endif //ZYPP_NG_REPORTHELPER_INCLUDED
