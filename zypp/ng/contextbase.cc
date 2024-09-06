#include "contextbase.h"
#include "zypp-core/zyppng/pipelines/mtry.h"
#include "zypp/KeyRing.h"
#include <zypp/Target.h>
#include "zypp/ZConfig.h"
#include "zypp/zypp_detail/ZyppLock.h"

namespace zyppng {


  ContextBase::ContextBase()
    : _tmpDir( zypp::filesystem::TmpPath::defaultLocation(), "zypp." )
  { }

  ContextBase::~ContextBase()
  { }

  zypp::Pathname ContextBase::defaultConfigPath()
  {
    return "/etc/zypp/zypp.conf";
  }

  zypp::Pathname ContextBase::contextRoot() const
  {
    //  this is a programming error, the C API will hide the 2 step create and init process.
    if ( !_settings )
      ZYPP_THROW (zypp::Exception("Uninitialized Context"));

    // target always wins, normally this should be same same as the settings root
    // but legacy mode can change the target
    if ( _target )
      return _target->root ();

    return _settings->root;
  }

  /*!
   * Legacy support, we change the target inside our context
   */
  void ContextBase::changeToTarget( const zypp::Pathname & root, bool doRebuild_r )
  {
    if ( _target && _target->root() == root ) {
        MIL << "Repeated call to changeToTarget()" << std::endl;
        return;
    }

    assertInitialized();
    finishTarget().unwrap ();
    _target = new zypp::Target( root, doRebuild_r );
    _sigTargetChanged.emit();

     _config->notifyTargetChanged( _target->root(), *_settings->configPath );
  }

  expected<zypp::ZyppContextLockRef> ContextBase::aquireLock()
  {
    auto myLock = zypp::ZyppContextLock::globalLock( _myRoot );
    try {
      myLock->acquireLock ();
    } catch (...) {
      return expected<zypp::ZyppContextLockRef>::error ( ZYPP_FWD_CURRENT_EXCPT() );
    }
    return expected<zypp::ZyppContextLockRef>::success ( std::move(myLock) );
  }

  expected<void> ContextBase::loadConfig( zypp::Pathname confPath )
  {
    try {
      _config.reset( new zypp::ZConfig(confPath) );
    } catch (...) {
      _config.reset();
      return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
    return expected<void>::success();
  }

  void ContextBase::assertInitialized()
  {
    //  this is a programming error, the C API will hide the 2 step create and init process.
    if ( !_settings )
      ZYPP_THROW (zypp::Exception("Uninitialized Context"));

    if ( !_config && !_settings->configPath ) {
      _settings->configPath = _settings->root / defaultConfigPath();
    }

    if ( !_keyRing ) {
      MIL << "Initializing keyring..." << std::endl;
      _keyRing = new KeyRing(tmpPath());
      _keyRing->allowPreload( true );
    }
  }

  expected<void> ContextBase::initialize( ContextSettings &&settings )
  {
    if ( _settings ) {
      return expected<void>::error( ZYPP_EXCPT_PTR (zypp::Exception("Context can not be initialized twice")) );
    }

    _settings = std::move(settings);

    try {
      assertInitialized ();
    } catch ( const zypp::Exception & ) {
      return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }

    using namespace zyppng::operators;

    return loadConfig( *_settings->configPath )
    // aquire the lock
    | and_then ( [this](){ return aquireLock(); } )
    // last step , remember the lock, init the target
    | and_then ( [this]( zypp::ZyppContextLockRef myLock ) {
      // init the target
      try {
        // everything worked , keep the lock
        _myLock = myLock;

        return expected<void>::success();
      } catch(...) {
        return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT () );
      }
    });
  }

  expected<void> ContextBase::initTarget()
  {
    try {

      if ( _target ) {
        MIL << "Repeated call to initTarget, ignoring." << std::endl;
        return expected<void>::success();
      }

      MIL << "context(" << this << ") initTarget( " << _myRoot << (_settings->rebuildDb ?", rebuilddb":"") << ")" << std::endl;
      _target = new zypp::Target( _myRoot, _settings->rebuildDb );
      return expected<void>::success();

    } catch ( ... ) {
      return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
  }

  expected<void> ContextBase::finishTarget()
  {
    try {
      _sigClose.emit(); // trigger target unload from the pools it might be loaded into
      return expected<void>::success();
    } catch ( ... ) {
      return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
  }

  void ContextBase::legacyInit( ContextSettings &&settings, zypp::ZConfigRef sharedConf )
  {
    if ( _settings ) {
      ZYPP_THROW (zypp::Exception("Context can not be initialized twice"));
    }
    _settings   = std::move(settings);
    _config     = std::move(sharedConf);
    _legacyMode = true;
  }

  KeyRingRef ContextBase::keyRing()
  {
    assertInitialized();
    return _keyRing;
  }

  zypp::ZConfig &ContextBase::config()
  {
    assertInitialized();
    // config loaded on demand if queried before initializing the target
    if ( !_config ) loadConfig ( *_settings->configPath ).unwrap();
    return *_config;
  }

  zypp::Pathname ContextBase::tmpPath() const
  {
    return _tmpDir.path();
  }

  TargetRef ContextBase::target() const
  {
    return _target;
  }

}
