#include "TestcaseSetupImpl.h"

#include <zypp/ZYpp.h>
#include <zypp/ZYppFactory.h>
#include <zypp/Resolver.h>
#include <zypp/ResPool.h>
#include <zypp/VendorAttr.h>
#include <zypp/target/modalias/Modalias.h>
#include <zypp/base/Algorithm.h>

namespace zypp::misc::testcase
{

  // ─── Helpers (migrated verbatim from deptestomatic, refactor in second pass) ─
  namespace {

    struct FindPackage
    {
      PoolItem poolItem;
      Resolvable::Kind kind;
      bool edition_set;
      Edition edition;
      bool arch_set;
      Arch arch;

      FindPackage( Resolvable::Kind k, const std::string & v,
        const std::string & r, const std::string & a )
        : kind( k )
        , edition_set( !v.empty() )
        , edition( v, r )
        , arch_set( !a.empty() )
        , arch( a )
      {}

      void _remember( PoolItem p ) { poolItem = p; }

      bool operator()( PoolItem p )
      {
        if ( arch_set && arch != p->arch() )
          return true;
        if ( !p->arch().compatibleWith( ZConfig::instance().systemArchitecture() ) )
          return true;
        if ( edition_set && p->edition().match( edition ) != 0 )
          return true;
        if ( !poolItem
             || poolItem->arch().compare( p->arch() ) < 0
             || poolItem->edition().compare( p->edition() ) < 0 )
          _remember( p );
        return true;
      }
    };

    static PoolItem get_poolItem( const std::string & source_alias,
      const std::string & package_name,
      const std::string & kind_name = "",
      const std::string & ver = "",
      const std::string & rel = "",
      const std::string & arch = "" )
    {
      PoolItem poolItem;
      ResPool pool = ResPool::instance();
      Resolvable::Kind kind = ResKind::fromBuiltin( kind_name );
      if ( kind == ResKind::nokind ) kind = ResKind::package;

      try {
        FindPackage info( kind, ver, rel, arch );

        invokeOnEach( pool.byIdentBegin( kind, package_name ),
          pool.byIdentEnd  ( kind, package_name ),
          resfilter::ByRepository( source_alias ),
          std::ref( info ) );

        poolItem = info.poolItem;
        if ( !poolItem ) {
          // Try all channels — useful for language packages.
          invokeOnEach( pool.byIdentBegin( kind, package_name ),
            pool.byIdentEnd  ( kind, package_name ),
            std::ref( info ) );
          poolItem = info.poolItem;
        }
      }
      catch ( const Exception & e ) {
        ZYPP_CAUGHT( e );
        WAR << "Can't find kind[" << kind_name << "]:'" << package_name
            << "': source '" << source_alias << "' not defined" << std::endl;
      }

      if ( !poolItem )
        WAR << "Can't find kind: " << kind << ":'" << package_name
            << "' in source '" << source_alias << "': no such name/kind" << std::endl;

      return poolItem;
    }

    void applyLockEntry(const TestcaseSetup::LockEntry &entry, ResPool &pool )
    {
      const bool isKeep = ( entry.first == "keep" );
      const auto & props = entry.second;

      auto getProp = [&]( const std::string & k ) -> std::string {
        auto it = props.find(k);
        return it != props.end() ? it->second : std::string();
      };

      if ( !isKeep ) {
        std::string source_alias = getProp ("channel");
        std::string package_name = getProp ("name");
        if (package_name.empty())
          package_name = getProp ("package");
        std::string kind_name = getProp ("kind");
        std::string version = getProp ("version");
        if ( version.empty() )
          version = getProp ("ver");
        std::string release = getProp ("release");
        if ( release.empty() )
          release = getProp ("rel");
        std::string architecture = getProp ("arch");

        if ( version.empty() )
        {
          if ( kind_name.empty() )
            kind_name = "package";
          ui::Selectable::Ptr item = ui::Selectable::get( ResKind(kind_name), package_name );
          if ( item )
          {
            // first set to protected, then to taboo so we run through both logic paths
            // to make sure that if there is a installed object the candidates are also locked
            item->setStatus( item->hasInstalledObj() ? ui::S_Protected : ui::S_Taboo );
            item->setStatus( ui::S_Taboo );
          }
          else
          {
            WAR << "Unknown Selectable " << kind_name << ":" << package_name << std::endl;
          }
        }
        else
        {
          PoolItem poolItem;
          poolItem = get_poolItem (source_alias, package_name, kind_name, version, release, architecture );
          if (poolItem) {
            MIL << "Locking " << package_name << " from channel " << source_alias << poolItem << std::endl;
            poolItem.status().setLock (true, ResStatus::USER);
          } else {
            WAR << "Unknown package " << source_alias << "::" << package_name << std::endl;
          }
        }

      } else {

        std::string kind_name = getProp ("kind");
        std::string name = getProp ("name");
        if (name.empty())
          name = getProp ("package");

        std::string source_alias = getProp ("channel");
        if (source_alias.empty())
          source_alias = "@System";

        if (name.empty())
        {
          WAR << "transact need 'name' parameter" << std::endl;
          return;
        }

        PoolItem poolItem;

        poolItem = get_poolItem( source_alias, name, kind_name, getProp ("version"), getProp ("release") );

        if (poolItem) {
          // first: set anything
          if (source_alias == "@System") {
            poolItem.status().setToBeUninstalled( ResStatus::USER );
          }
          else {
            poolItem.status().setToBeInstalled( ResStatus::USER );
          }
          // second: keep old state
          poolItem.status().setTransact( false, ResStatus::USER );
        }
        else {
          WAR << "Unknown item " << source_alias << "::" << name << std::endl;
        }
      }
    }
  }

  // ─────────────────────────────────────────────────────────────────────────────

  RepoData::RepoData() : _pimpl( new RepoDataImpl )
  {}

  RepoData::~RepoData()
  { }

  RepoData::RepoData(RepoDataImpl &&data) : _pimpl( new RepoDataImpl( std::move(data)) )
  { }

  TestcaseRepoType RepoData::type() const
  { return _pimpl->type; }

  const std::string &RepoData::alias() const
  { return _pimpl->alias; }

  uint RepoData::priority() const
  { return _pimpl->priority; }

  const std::string &RepoData::path() const
  { return _pimpl->path; }

  const RepoDataImpl &RepoData::data() const
  { return *_pimpl; }

  RepoDataImpl &RepoData::data()
  { return *_pimpl; }

  ForceInstall::ForceInstall() : _pimpl( new ForceInstallImpl )
  { }

  ForceInstall::~ForceInstall()
  { }

  ForceInstall::ForceInstall(ForceInstallImpl &&data) : _pimpl( new ForceInstallImpl( std::move(data) ))
  { }

  const ForceInstallImpl &ForceInstall::data() const
  { return *_pimpl; }

  ForceInstallImpl &ForceInstall::data()
  { return *_pimpl; }

  const std::string &ForceInstall::channel() const
  { return _pimpl->channel; }

  const std::string &ForceInstall::package() const
  { return _pimpl->package; }

  const std::string &ForceInstall::kind() const
  { return _pimpl->kind; }

  TestcaseSetup::TestcaseSetup() : _pimpl( new TestcaseSetupImpl )
  { }

  TestcaseSetup::~TestcaseSetup()
  { }

  Arch TestcaseSetup::architecture() const
  { return _pimpl->architecture; }

  const std::optional<RepoData> &TestcaseSetup::systemRepo() const
  { return _pimpl->systemRepo; }

  const std::vector<RepoData> &TestcaseSetup::repos() const
  { return _pimpl->repos; }

  ResolverFocus TestcaseSetup::resolverFocus() const
  { return _pimpl->resolverFocus; }

  const zypp::filesystem::Pathname &TestcaseSetup::globalPath() const
  { return _pimpl->globalPath; }

  const zypp::filesystem::Pathname &TestcaseSetup::hardwareInfoFile() const
  { return _pimpl->hardwareInfoFile; }

  const zypp::filesystem::Pathname &TestcaseSetup::systemCheck() const
  { return _pimpl->systemCheck; }

  const target::Modalias::ModaliasList &TestcaseSetup::modaliasList() const
  { return _pimpl->modaliasList; }

  const base::SetTracker<LocaleSet> &TestcaseSetup::localesTracker() const
  { return _pimpl->localesTracker; }

  const std::vector<std::vector<std::string> > &TestcaseSetup::vendorLists() const
  { return _pimpl->vendorLists; }

  const sat::StringQueue &TestcaseSetup::autoinstalled() const
  { return _pimpl->autoinstalled; }

  const std::set<std::string> &TestcaseSetup::multiversionSpec() const
  { return _pimpl->multiversionSpec; }

  const std::vector<ForceInstall> &TestcaseSetup::forceInstallTasks() const
  { return _pimpl->forceInstallTasks; }

  const std::vector<TestcaseSetup::LockEntry> &TestcaseSetup::locks() const
  { return _pimpl->locks; }

  bool TestcaseSetup::set_licence() const
  { return _pimpl->set_licence; }

  bool TestcaseSetup::show_mediaid() const
  { return _pimpl->show_mediaid; }

  bool TestcaseSetup::ignorealreadyrecommended() const
  { return _pimpl->ignorealreadyrecommended; }

  bool TestcaseSetup::onlyRequires() const
  { return _pimpl->onlyRequires; }

  bool TestcaseSetup::forceResolve() const
  { return _pimpl->forceResolve; }

  bool TestcaseSetup::cleandepsOnRemove() const
  { return _pimpl->cleandepsOnRemove; }

  bool TestcaseSetup::noUpdateProvide() const
  { return _pimpl->noUpdateProvide; }

  bool TestcaseSetup::allowDowngrade() const
  { return _pimpl->allowDowngrade; }

  bool TestcaseSetup::allowNameChange() const
  { return _pimpl->allowNameChange; }

  bool TestcaseSetup::allowArchChange() const
  { return _pimpl->allowArchChange; }

  bool TestcaseSetup::allowVendorChange() const
  { return _pimpl->allowVendorChange; }

  bool TestcaseSetup::dupAllowDowngrade() const
  { return _pimpl->dupAllowDowngrade; }

  bool TestcaseSetup::dupAllowNameChange() const
  { return _pimpl->dupAllowNameChange; }

  bool TestcaseSetup::dupAllowArchChange() const
  { return _pimpl->dupAllowArchChange; }

  bool TestcaseSetup::dupAllowVendorChange() const
  { return _pimpl->dupAllowVendorChange; }

  bool TestcaseSetup::applySetup( zypp::RepoManager &manager ) const
  {
    const auto &setup = data();
    if ( !setup.architecture.empty() )
    {
      MIL << "Setting architecture to '" << setup.architecture << "'" << std::endl;
      ZConfig::instance().setSystemArchitecture( setup.architecture );
      setenv ("ZYPP_TESTSUITE_FAKE_ARCH", setup.architecture.c_str(), 1);
    }

    if ( setup.systemRepo ) {
      if (!loadRepo( manager, *this, *setup.systemRepo ) )
      {
        ERR << "Can't setup 'system'" << std::endl;
        return false;
      }
    }

    if ( !setup.hardwareInfoFile.empty() ) {
      setenv( "ZYPP_MODALIAS_SYSFS", setup.hardwareInfoFile.asString().c_str(), 1 );
      MIL << "setting HardwareInfo to: " << setup.hardwareInfoFile.asString() << std::endl;
    }

    for ( const auto &channel : setup.repos ) {
      if ( !loadRepo( manager, *this, channel )  )
      {
        ERR << "Can't setup 'channel'" << std::endl;
        return false;
      }
    }

    if ( !setup.systemCheck.empty() ) {
      MIL << "setting systemCheck to: " << setup.systemCheck.asString() << std::endl;
      SystemCheck::instance().setFile( setup.systemCheck );
    }

    return true;
  }

  bool TestcaseSetup::loadRepo( zypp::RepoManager &manager, const TestcaseSetup &setup, const RepoData &data )
  {
    const auto &repoData = data.data();
    Pathname pathname = setup._pimpl->globalPath + repoData.path;
    MIL << "'" << pathname << "'" << std::endl;

    Repository repo;

    using TrType = zypp::misc::testcase::TestcaseRepoType;

    if ( repoData.type == TrType::Url ) {
      try {
        MIL << "Load from Url '" << repoData.path << "'" << std::endl;

        RepoInfo nrepo;
        nrepo.setAlias      ( repoData.alias );
        nrepo.setName       ( repoData.alias );
        nrepo.setEnabled    ( true );
        nrepo.setAutorefresh( false );
        nrepo.setPriority   ( repoData.priority );

        Url repoUrl( repoData.path );
        if ( repoUrl.getScheme() == "testcase" )
        {
          // testcase:<relative-path> — a repo bundled alongside the testcase
          // fixture itself (e.g. a local susetags/rpm-md directory), resolved
          // against the testcase directory. Any other scheme (http://, dir:,
          // ftp://, ...) is used verbatim, unchanged from prior behaviour.
          repoUrl = ( setup._pimpl->globalPath / repoUrl.getPathName() ).asUrl();
          // Bundled fixtures are never signed — same precedent as
          // zypp-logic/tests/lib/TestSetup.h::loadRepo(), which disables
          // gpgCheck for its own local test repos. Absolute URLs (any other
          // scheme) are untouched: a testcase pointing at a real repo keeps
          // whatever signature checking it would normally get.
          nrepo.setGpgCheck( false );
        }
        nrepo.addBaseUrl( repoUrl );

        manager.refreshMetadata( nrepo );
        manager.buildCache( nrepo );
        manager.loadFromCache( nrepo );
      }
      catch ( Exception & excpt_r ) {
        ZYPP_CAUGHT (excpt_r);
        ERR << "Couldn't load packages from Url '" << repoData.path << "'" << std::endl;
        return false;
      }
    }
    else {
      try {
        MIL << "Load from File '" << pathname << "'" << std::endl;
        zypp::Repository satRepo;

        if ( repoData.alias == "@System" ) {
          satRepo = zypp::sat::Pool::instance().systemRepo();
        } else {
          satRepo = zypp::sat::Pool::instance().reposInsert( repoData.alias );
        }

        RepoInfo nrepo;

        nrepo.setAlias      ( repoData.alias );
        nrepo.setName       ( repoData.alias );
        nrepo.setEnabled    ( true );
        nrepo.setAutorefresh( false );
        nrepo.setPriority   ( repoData.priority );
        nrepo.addBaseUrl   ( pathname.asUrl() );

        satRepo.setInfo (nrepo);
        if ( repoData.type == TrType::Helix )
          satRepo.addHelix( pathname );
        else
          satRepo.addTesttags( pathname );
        MIL << "Loaded " << satRepo.solvablesSize() << " resolvables from " << ( repoData.path.empty()?pathname.asString():repoData.path) << "." << std::endl;
      }
      catch ( Exception & excpt_r ) {
        ZYPP_CAUGHT (excpt_r);
        ERR << "Couldn't load packages from XML file '" << repoData.path << "'" << std::endl;
        return false;
      }
    }
    return true;
  }

  TestcaseSetupImpl &TestcaseSetup::data()
  {
    return *_pimpl;
  }

  const TestcaseSetupImpl &TestcaseSetup::data() const
  {
    return *_pimpl;
  }

  bool TestcaseSetup::applySetup( zypp::RepoManager &manager, ApplySetupFlags flags_r ) const
  {
    // Repos/arch/hardware are always applied via the existing overload.
    if ( !applySetup( manager ) )
      return false;

    if ( flags_r.testFlag( AS_LOCALES ) )
    {
      base::SetTracker<LocaleSet> lt = localesTracker();
      lt.removed().insert( lt.current().begin(), lt.current().end() );
      sat::Pool::instance().initRequestedLocales( lt.removed() );
      lt.added().insert( lt.current().begin(), lt.current().end() );
      sat::Pool::instance().setRequestedLocales( lt.added() );
    }

    if ( flags_r.testFlag( AS_AUTOINSTALLED ) )
    {
      sat::Pool::instance().setAutoInstalled( autoinstalled() );
    }

    if ( flags_r.testFlag( AS_VENDOR_LISTS ) )
    {
      for ( const auto & vlist : vendorLists() )
        VendorAttr::noTargetInstance().addVendorList( vlist );
    }

    if ( flags_r.testFlag( AS_MODALIAS ) )
    {
      target::Modalias::instance().modaliasList( modaliasList() );
    }

    if ( flags_r.testFlag( AS_MULTIVERSION ) )
    {
      ZConfig::instance().multiversionSpec( multiversionSpec() );
    }

    if ( flags_r.testFlag( AS_LOCKS ) )
    {
      ResPool pool = ResPool::instance();
      for ( const auto & entry : locks() )
        applyLockEntry( entry, pool );
    }

    if ( flags_r.testFlag( AS_SOLVER_FLAGS ) )
    {
      Resolver_Ptr resolver = getZYpp()->resolver();
      resolver->setFocus                   ( resolverFocus()             );
      resolver->setIgnoreAlreadyRecommended( ignorealreadyrecommended()  );
      resolver->setOnlyRequires            ( onlyRequires()              );
      resolver->setForceResolve            ( forceResolve()              );
      resolver->setCleandepsOnRemove       ( cleandepsOnRemove()         );
      resolver->setAllowDowngrade          ( allowDowngrade()            );
      resolver->setAllowNameChange         ( allowNameChange()           );
      resolver->setAllowArchChange         ( allowArchChange()           );
      resolver->setAllowVendorChange       ( allowVendorChange()         );
      resolver->dupSetAllowDowngrade       ( dupAllowDowngrade()         );
      resolver->dupSetAllowNameChange      ( dupAllowNameChange()        );
      resolver->dupSetAllowArchChange      ( dupAllowArchChange()        );
      resolver->dupSetAllowVendorChange    ( dupAllowVendorChange()      );
    }

    return true;
  }
}
