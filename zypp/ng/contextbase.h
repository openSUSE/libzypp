/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_CONTEXTBASE_INCLUDED
#define ZYPP_NG_CONTEXTBASE_INCLUDED

#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/zyppng/pipelines/expected.h>
#include <zypp-media/ng/MediaContext>
#include <zypp/ng/repo/repovariablescache.h>

namespace zypp {
  DEFINE_PTR_TYPE(KeyRing);
  DEFINE_PTR_TYPE(Target);
  ZYPP_FWD_DECL_TYPE_WITH_REFS( ZyppContextLock );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( ZConfig );
  class ZConfig;
}


namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (ContextBase);

  using KeyRing    = zypp::KeyRing;
  using KeyRingRef = zypp::KeyRing_Ptr;

  using Target     = zypp::Target;
  using TargetRef  = zypp::Target_Ptr;

  struct ContextSettings {
    zypp::Pathname root = "/";
    bool rebuildDb = false;
    std::optional<zypp::Pathname> configPath;
  };

  class ContextBase : public MediaContext
  {
  public:
    ~ContextBase() override;
    ContextBase(const ContextBase &) = delete;
    ContextBase(ContextBase &&) = delete;
    ContextBase &operator=(const ContextBase &) = delete;
    ContextBase &operator=(ContextBase &&) = delete;

    /*!
     * Returns the root path of the context
     */
    zypp::Pathname contextRoot() const override;

    /*!
     * Gets the zypp lock, loads the config and sets up keyring
     */
    expected<void> initialize(  ContextSettings &&settings = {} );


    /*!
     * Initialize the target, this will trigger file creation
     * in the context root, for example the rpm database.
     */
    expected<void> initTarget ();

    /*!
     * Request the target to be released from all pools it is currently
     * part of.
     */
    expected<void> finishTarget();

    KeyRingRef keyRing ();
    zypp::ZConfig &config();
    zypp::MediaConfig &mediaConfig() override;
    zypp::Pathname tmpPath() const;
    TargetRef target() const;


    /*!
     * Tries to resolve the variable \a var from the repoCache,
     * returns a pointer to the found value or nullptr if the variable
     * is not known.
     */
    const std::string * resolveRepoVar( const std::string & var );

    repo::RepoVarsMap &repoVarCache();
    const repo::RepoVarsMap &repoVarCache() const;

    /*!
     * Resets all cached repo variables
     */
    void clearRepoVariables();

    /*!
     * Signal emitted during context close, e.g. unloading the target.
     * All classes depending on the context can connect to it to clean up.
     * E.g. FusionPool uses it to know when a target that is loaded into the pool
     * is shutting down, so it can be automatically unloaded
     */
    SignalProxy<void()> sigClose() {
      return _sigClose;
    }

  protected:
    ContextBase();

    // legacy support functions, we need those for supporting the
    // Zypp pointer and other legacy things. Do not use in logic code

    /*!
     * Partially initializes the context, sets up keyring and assigns the global config
     * reference, but does not yet boot the target. In legacy mode changeToTarget
     * needs to be called as well by the Zypp instance
     */
    void legacyInit( ContextSettings &&settings, zypp::ZConfigRef sharedConf );

    /*!
     * Legacy support, might throw. This changes the internaly used zypp::Target.
     */
    void changeToTarget( const zypp::Pathname &root , bool doRebuild_r );

    SignalProxy<void()> sigTargetChanged() {
      return _sigTargetChanged;
    }

  private:

    /**
     * \todo move CredentialManager here
     */

    std::optional<ContextSettings> _settings;
    bool _legacyMode = false; // set by legacyInit. Will disable locking inside the context, Zypp and ZyppFactory take care of that

    Signal<void()> _sigClose;
    Signal<void()> _sigTargetChanged; // legacy support signal in case the context changes its target

    expected<zypp::ZyppContextLockRef> aquireLock();
    expected<void> loadConfig( zypp::Pathname confPath );
    void assertInitialized();


    zypp::Pathname _myRoot;
    zypp::ZConfigRef _config;

    zypp::filesystem::TmpDir _tmpDir;
    KeyRingRef _keyRing;
    TargetRef  _target;

    zypp::ZyppContextLockRef _myLock;
    repo::RepoVarsMap _repoVarCache;

  };


}


#endif
