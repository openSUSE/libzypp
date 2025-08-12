/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_KEYRINGWORKFLOW_INCLUDED
#define ZYPP_NG_KEYRINGWORKFLOW_INCLUDED

#include <zypp-core/ng/pipelines/AsyncResult>
#include <zypp-core/ng/pipelines/Expected>

namespace zypp {
  class RepoInfo;
  DEFINE_PTR_TYPE(KeyRing);

  namespace keyring {
    class VerifyFileContext;
  }

}

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (Context);

  /*!
   * \namespace KeyRingWorkflow
   * Contains all KeyRing related workflows.
   *
   * \sa zypp::KeyRing
   */
  namespace KeyRingWorkflow {

    /**
     * Try to find the \a id in key cache or repository specified in \a info. Ask the user to trust
     * the key if it was found
     */
    MaybeAwaitable<bool> provideAndImportKeyFromRepository(ContextRef ctx, std::string id_r, zypp::RepoInfo info_r );

    /**
     * Follows a signature verification interacting with the user.
     * The bool returned depends on user decision to trust or not.
     *
     * To propagate user decisions, either connect to the \ref KeyRingReport
     * or use its static methods to set the desired defaults.
     *
     * A second bool passed as reference arg \a sigValid_r tells whether the
     * signature was actually successfully verified. If \a sigValid_r returns
     * \c false, but the method \c true, you know it's due to user callback or
     * defaults.
     *
     * \code
     * struct KeyRingReportReceive : public callback::ReceiveReport<KeyRingReport>
     * {
     *   KeyRingReportReceive() { connect(); }
     *
     *   // Overload the virtual methods to return the appropriate values.
     *   virtual bool askUserToAcceptUnsignedFile( const std::string &file );
     *   ...
     * };
     * \endcode
     *
     * \param keyRing The KeyRing instance the file verification should run against
     * \param context_r The verification context containing all required informations
     *
     * \returns a std::pair, containing a boolean signalling if the verification was successful or not and the \ref zypp::keyring::VerifyFileContext containing
     *          more detailed informations.
     *
     * \see \ref zypp::KeyRingReport
     */
    MaybeAwaitable<std::pair<bool,zypp::keyring::VerifyFileContext>> verifyFileSignature( ContextRef zyppContext, zypp::keyring::VerifyFileContext && context_r );

    MaybeAwaitable<std::pair<bool,zypp::keyring::VerifyFileContext>> verifyFileSignature( ContextRef zyppContext, zypp::KeyRing_Ptr keyRing, zypp::keyring::VerifyFileContext &&context_r );
  }
}

#endif
