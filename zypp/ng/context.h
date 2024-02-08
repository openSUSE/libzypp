/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_CONTEXT_INCLUDED
#define ZYPP_NG_CONTEXT_INCLUDED

#include <zypp-core/zyppng/async/AsyncOp>
#include <zypp-core/zyppng/ui/UserInterface>
#include <zypp/RepoManager.h>

namespace zypp {
  DEFINE_PTR_TYPE(KeyRing);
  class ZConfig;
}

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( Context );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( ProgressObserver );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( Provide );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( RepoManager );

  using KeyRing    = zypp::KeyRing;
  using KeyRingRef = zypp::KeyRing_Ptr;

  class ContextPrivate;


  /*!
   * The Context class is the central object of libzypp, carrying all state that is
   * required to manage the system.
   */
  class Context : public UserInterface
  {
    ZYPP_DECLARE_PRIVATE(Context)
    ZYPP_ADD_CREATE_FUNC(Context)

  public:

    using ProvideType = Provide;

    ZYPP_DECL_PRIVATE_CONSTR(Context);

    template <typename AsyncRes>
    void execute ( AsyncOpRef<AsyncRes> op ) {
      if ( op->isReady () )
        return;
      return executeImpl( op );
    }

    ProvideRef provider() const;
    KeyRingRef keyRing () const;
    zypp::ZConfig &config();

  private:
    void executeImpl ( const AsyncOpBaseRef& op );
  };

}


#endif
