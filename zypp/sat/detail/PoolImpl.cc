/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/detail/PoolImpl.cc
 *
*/
#include <iostream>
#include <fstream>
#include <boost/mpl/int.hpp>

#include "zypp/base/Easy.h"
#include "zypp/base/LogTools.h"
#include "zypp/base/Gettext.h"
#include "zypp/base/Exception.h"
#include "zypp/base/Measure.h"
#include "zypp/base/WatchFile.h"
#include "zypp/base/Sysconfig.h"
#include "zypp/base/IOStream.h"

#include "zypp/ZConfig.h"

#include "zypp/sat/detail/PoolImpl.h"
#include "zypp/sat/SolvableSet.h"
#include "zypp/sat/Pool.h"
#include "zypp/Capability.h"
#include "zypp/Locale.h"
#include "zypp/PoolItem.h"

#include "zypp/target/modalias/Modalias.h"
#include "zypp/media/MediaPriority.h"

extern "C"
{
// Workaround libsolv project not providing a common include
// directory. (the -devel package does, but the git repo doesn't).
// #include <solv/repo_helix.h>
int repo_add_helix( ::Repo *repo, FILE *fp, int flags );
}

using std::endl;

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::satpool"

// ///////////////////////////////////////////////////////////////////
namespace zypp
{
  /////////////////////////////////////////////////////////////////
  namespace env
  {
    /**  */
    inline int LIBSOLV_DEBUGMASK()
    {
      const char * envp = getenv("LIBSOLV_DEBUGMASK");
      return envp ? str::strtonum<int>( envp ) : 0;
    }
  } // namespace env
  ///////////////////////////////////////////////////////////////////
  namespace sat
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    namespace detail
    { /////////////////////////////////////////////////////////////////

      // MPL checks for satlib constants we redefine to avoid
      // includes and defines.
      BOOST_MPL_ASSERT_RELATION( noId,                 ==, STRID_NULL );
      BOOST_MPL_ASSERT_RELATION( emptyId,              ==, STRID_EMPTY );

      BOOST_MPL_ASSERT_RELATION( noSolvableId,         ==, ID_NULL );
      BOOST_MPL_ASSERT_RELATION( systemSolvableId,     ==, SYSTEMSOLVABLE );

      BOOST_MPL_ASSERT_RELATION( solvablePrereqMarker, ==, SOLVABLE_PREREQMARKER );
      BOOST_MPL_ASSERT_RELATION( solvableFileMarker,   ==, SOLVABLE_FILEMARKER );

      BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_AND,       ==, REL_AND );
      BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_OR,        ==, REL_OR );
      BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_WITH,      ==, REL_WITH );
      BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_NAMESPACE, ==, REL_NAMESPACE );
      BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_ARCH,      ==, REL_ARCH );

      BOOST_MPL_ASSERT_RELATION( namespaceModalias,	==, NAMESPACE_MODALIAS );
      BOOST_MPL_ASSERT_RELATION( namespaceLanguage,	==, NAMESPACE_LANGUAGE );
      BOOST_MPL_ASSERT_RELATION( namespaceFilesystem,	==, NAMESPACE_FILESYSTEM );

      /////////////////////////////////////////////////////////////////

      const std::string & PoolImpl::systemRepoAlias()
      {
        static const std::string _val( "@System" );
        return _val;
      }

      const Pathname & sysconfigStoragePath()
      {
	static const Pathname _val( "/etc/sysconfig/storage" );
	return _val;
      }

      /////////////////////////////////////////////////////////////////

      static void logSat( CPool *, void *data, int type, const char *logString )
      {
	//                            "1234567890123456789012345678901234567890
	if ( 0 == strncmp( logString, "job: user installed", 19 ) )
	  return;
	if ( 0 == strncmp( logString, "job: multiversion", 17 ) )
	  return;
	if ( 0 == strncmp( logString, "  - no rule created", 19 ) )
	  return;
	if ( 0 == strncmp( logString, "    next rules: 0 0", 19 ) )
	  return;

	if ( type & (SOLV_FATAL|SOLV_ERROR) ) {
	  L_ERR("libsolv") << logString;
	} else if ( type & SOLV_DEBUG_STATS ) {
	  L_DBG("libsolv") << logString;
	} else {
	  L_MIL("libsolv") << logString;
	}
      }

      detail::IdType PoolImpl::nsCallback( CPool *, void * data, detail::IdType lhs, detail::IdType rhs )
      {
        // lhs:    the namespace identifier, e.g. NAMESPACE:MODALIAS
        // rhs:    the value, e.g. pci:v0000104Cd0000840[01]sv*sd*bc*sc*i*
        // return: 0 if not supportded
        //         1 if supported by the system
        //        -1  AFAIK it's also possible to return a list of solvables that support it, but don't know how.

        static const detail::IdType RET_unsupported    = 0;
        static const detail::IdType RET_systemProperty = 1;
        switch ( lhs )
        {
          case NAMESPACE_LANGUAGE:
          {
	    const TrackedLocaleIds & localeIds( reinterpret_cast<PoolImpl*>(data)->trackedLocaleIds() );
	    return localeIds.contains( IdString(rhs) ) ? RET_systemProperty : RET_unsupported;
          }
          break;

          case NAMESPACE_MODALIAS:
          {
            // modalias strings in capability may be hexencoded because rpm does not allow
            // ',', ' ' or other special chars.
            return target::Modalias::instance().query( str::hexdecode( IdString(rhs).c_str() ) )
                ? RET_systemProperty
              : RET_unsupported;
          }
          break;

          case NAMESPACE_FILESYSTEM:
          {
	    const std::set<std::string> & requiredFilesystems( reinterpret_cast<PoolImpl*>(data)->requiredFilesystems() );
            return requiredFilesystems.find( IdString(rhs).asString() ) != requiredFilesystems.end() ? RET_systemProperty : RET_unsupported;
          }
          break;

        }

        WAR << "Unhandled " << Capability( lhs ) << " vs. " << Capability( rhs ) << endl;
        return RET_unsupported;
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : PoolMember::myPool
      //	METHOD TYPE : PoolImpl
      //
      PoolImpl & PoolMember::myPool()
      {
        static PoolImpl _global;
        return _global;
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : PoolImpl::PoolImpl
      //	METHOD TYPE : Ctor
      //
      PoolImpl::PoolImpl()
      : _pool( ::pool_create() )
      {
        MIL << "Creating sat-pool." << endl;
        if ( ! _pool )
        {
          ZYPP_THROW( Exception( _("Can not create sat-pool.") ) );
        }
        // by now we support only a RPM backend
        ::pool_setdisttype(_pool, DISTTYPE_RPM );

        // initialialize logging
	if ( env::LIBSOLV_DEBUGMASK() )
	{
	  ::pool_setdebugmask(_pool, env::LIBSOLV_DEBUGMASK() );
	}
	else
	{
	  if ( getenv("ZYPP_LIBSOLV_FULLLOG") || getenv("ZYPP_LIBSAT_FULLLOG") )
	    ::pool_setdebuglevel( _pool, 3 );
	  else if ( getenv("ZYPP_FULLLOG") )
	    ::pool_setdebuglevel( _pool, 2 );
	  else
	    ::pool_setdebugmask(_pool, SOLV_DEBUG_JOB|SOLV_DEBUG_STATS );
	}

        ::pool_setdebugcallback( _pool, logSat, NULL );

        // set namespace callback
        _pool->nscallback = &nsCallback;
        _pool->nscallbackdata = (void*)this;
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : PoolImpl::~PoolImpl
      //	METHOD TYPE : Dtor
      //
      PoolImpl::~PoolImpl()
      {
        ::pool_free( _pool );
      }

     ///////////////////////////////////////////////////////////////////

      void PoolImpl::setDirty( const char * a1, const char * a2, const char * a3 )
      {
        if ( a1 )
        {
          if      ( a3 ) MIL << a1 << " " << a2 << " " << a3 << endl;
          else if ( a2 ) MIL << a1 << " " << a2 << endl;
          else           MIL << a1 << endl;
        }
        _serial.setDirty();           // pool content change
        _availableLocalesPtr.reset(); // available locales may change
        _multiversionListPtr.reset(); // re-evaluate ZConfig::multiversionSpec.
	_needrebootSpec.setDirty();   // re-evaluate needrebootSpec

        depSetDirty();	// invaldate dependency/namespace related indices
      }

      void PoolImpl::localeSetDirty( const char * a1, const char * a2, const char * a3 )
      {
        if ( a1 )
        {
          if      ( a3 ) MIL << a1 << " " << a2 << " " << a3 << endl;
          else if ( a2 ) MIL << a1 << " " << a2 << endl;
          else           MIL << a1 << endl;
        }
        _trackedLocaleIdsPtr.reset();	// requested locales changed
        depSetDirty();	// invaldate dependency/namespace related indices
      }

      void PoolImpl::depSetDirty( const char * a1, const char * a2, const char * a3 )
      {
        if ( a1 )
        {
          if      ( a3 ) MIL << a1 << " " << a2 << " " << a3 << endl;
          else if ( a2 ) MIL << a1 << " " << a2 << endl;
          else           MIL << a1 << endl;
        }
        ::pool_freewhatprovides( _pool );
      }

      void PoolImpl::prepare() const
      {
	// additional /etc/sysconfig/storage check:
	static WatchFile sysconfigFile( sysconfigStoragePath(), WatchFile::NO_INIT );
	if ( sysconfigFile.hasChanged() )
	{
	  _requiredFilesystemsPtr.reset(); // recreated on demand
	  const_cast<PoolImpl*>(this)->depSetDirty( "/etc/sysconfig/storage change" );
	}
	if ( _watcher.remember( _serial ) )
        {
          // After repo/solvable add/remove:
          // set pool architecture
          ::pool_setarch( _pool,  ZConfig::instance().systemArchitecture().asString().c_str() );
        }
        if ( ! _pool->whatprovides )
        {
          MIL << "pool_createwhatprovides..." << endl;

          ::pool_addfileprovides( _pool );
          ::pool_createwhatprovides( _pool );
        }
        if ( ! _pool->languages )
        {
	  // initial seting
	  const_cast<PoolImpl*>(this)->setTextLocale( ZConfig::instance().textLocale() );
        }
      }

      ///////////////////////////////////////////////////////////////////

      CRepo * PoolImpl::_createRepo( const std::string & name_r )
      {
        setDirty(__FUNCTION__, name_r.c_str() );
        CRepo * ret = ::repo_create( _pool, name_r.c_str() );
        if ( ret && name_r == systemRepoAlias() )
          ::pool_set_installed( _pool, ret );
        return ret;
      }

      void PoolImpl::_deleteRepo( CRepo * repo_r )
      {
        setDirty(__FUNCTION__, repo_r->name );
	if ( isSystemRepo( repo_r ) )
	  _autoinstalled.clear();
        eraseRepoInfo( repo_r );
        ::repo_free( repo_r, /*resusePoolIDs*/false );
	// If the last repo is removed clear the pool to actually reuse all IDs.
	// NOTE: the explicit ::repo_free above asserts all solvables are memset(0)!
	if ( !_pool->urepos )
	{
	  _serialIDs.setDirty();	// Indicate resusePoolIDs - ResPool must also invalidate it's PoolItems
	  ::pool_freeallrepos( _pool, /*resusePoolIDs*/true );
	}
      }

      int PoolImpl::_addSolv( CRepo * repo_r, FILE * file_r )
      {
        setDirty(__FUNCTION__, repo_r->name );
        int ret = ::repo_add_solv( repo_r, file_r, 0 );
        if ( ret == 0 )
          _postRepoAdd( repo_r );
        return ret;
      }

      int PoolImpl::_addHelix( CRepo * repo_r, FILE * file_r )
      {
        setDirty(__FUNCTION__, repo_r->name );
        int ret = ::repo_add_helix( repo_r, file_r, 0 );
        if ( ret == 0 )
          _postRepoAdd( repo_r );
        return 0;
      }

      void PoolImpl::_postRepoAdd( CRepo * repo_r )
      {
        if ( ! isSystemRepo( repo_r ) )
        {
            // Filter out unwanted archs
          std::set<detail::IdType> sysids;
          {
            Arch::CompatSet sysarchs( Arch::compatSet( ZConfig::instance().systemArchitecture() ) );
            for_( it, sysarchs.begin(), sysarchs.end() )
              sysids.insert( it->id() );

              // unfortunately libsolv treats src/nosrc as architecture:
            sysids.insert( ARCH_SRC );
            sysids.insert( ARCH_NOSRC );
          }

          detail::IdType blockBegin = 0;
          unsigned       blockSize  = 0;
          for ( detail::IdType i = repo_r->start; i < repo_r->end; ++i )
          {
              CSolvable * s( _pool->solvables + i );
              if ( s->repo == repo_r && sysids.find( s->arch ) == sysids.end() )
              {
                // Remember an unwanted arch entry:
                if ( ! blockBegin )
                  blockBegin = i;
                ++blockSize;
              }
              else if ( blockSize )
              {
                // Free remembered entries
                  ::repo_free_solvable_block( repo_r, blockBegin, blockSize, /*resusePoolIDs*/false );
                  blockBegin = blockSize = 0;
              }
          }
          if ( blockSize )
          {
              // Free remembered entries
              ::repo_free_solvable_block( repo_r, blockBegin, blockSize, /*resusePoolIDs*/false );
              blockBegin = blockSize = 0;
          }
        }
      }

      detail::SolvableIdType PoolImpl::_addSolvables( CRepo * repo_r, unsigned count_r )
      {
        setDirty(__FUNCTION__, repo_r->name );
        return ::repo_add_solvable_block( repo_r, count_r );
      }

      void PoolImpl::setRepoInfo( RepoIdType id_r, const RepoInfo & info_r )
      {
        CRepo * repo( getRepo( id_r ) );
        if ( repo )
        {
          bool dirty = false;

          // libsolv priority is based on '<', while yum's repoinfo
          // uses 1(highest)->99(lowest). Thus we use -info_r.priority.
          if ( repo->priority != int(-info_r.priority()) )
          {
            repo->priority = -info_r.priority();
            dirty = true;
          }

 	  // subpriority is used to e.g. prefer http over dvd iff
	  // both have same priority.
          int mediaPriority( media::MediaPriority( info_r.url() ) );
          if ( repo->subpriority != mediaPriority )
          {
            repo->subpriority = mediaPriority;
            dirty = true;
          }

          if ( dirty )
            setDirty(__FUNCTION__, info_r.alias().c_str() );
        }
        _repoinfos[id_r] = info_r;
      }

      ///////////////////////////////////////////////////////////////////

      void PoolImpl::setTextLocale( const Locale & locale_r )
      {
	if ( ! locale_r )
	{
	  // We need one, so "en" is the last resort
	  const char *needone[] { "en" };
	  ::pool_set_languages( _pool, needone, 1 );
	}

	std::vector<std::string> fallbacklist;
	for ( Locale l( locale_r ); l; l = l.fallback() )
	{
	  fallbacklist.push_back( l.code() );
	}
	dumpRangeLine( MIL << "pool_set_languages: ", fallbacklist.begin(), fallbacklist.end() ) << endl;

	std::vector<const char *> fallbacklist_cstr;
	for_( it, fallbacklist.begin(), fallbacklist.end() )
	{
	  fallbacklist_cstr.push_back( it->c_str() );
	}
	::pool_set_languages( _pool, &fallbacklist_cstr.front(), fallbacklist_cstr.size() );
      }

      void PoolImpl::initRequestedLocales( const LocaleSet & locales_r )
      {
	if ( _requestedLocalesTracker.setInitial( locales_r ) )
	{
	  localeSetDirty( "initRequestedLocales" );
	  MIL << "Init RequestedLocales: " << _requestedLocalesTracker << " =" << locales_r << endl;
	}
      }

      void PoolImpl::setRequestedLocales( const LocaleSet & locales_r )
      {
	if ( _requestedLocalesTracker.set( locales_r ) )
	{
	  localeSetDirty( "setRequestedLocales" );
	  MIL << "New RequestedLocales: " << _requestedLocalesTracker << " =" << locales_r << endl;
	}
      }

      bool PoolImpl::addRequestedLocale( const Locale & locale_r )
      {
	bool done = _requestedLocalesTracker.add( locale_r );
        if ( done )
        {
          localeSetDirty( "addRequestedLocale", locale_r.code().c_str() );
	  MIL << "New RequestedLocales: " << _requestedLocalesTracker << " +" << locale_r << endl;
        }
        return done;
      }

      bool PoolImpl::eraseRequestedLocale( const Locale & locale_r )
      {
	bool done = _requestedLocalesTracker.remove( locale_r );
        if ( done )
        {
          localeSetDirty( "addRequestedLocale", locale_r.code().c_str() );
	  MIL << "New RequestedLocales: " << _requestedLocalesTracker << " -" << locale_r << endl;
        }
        return done;
      }


      const PoolImpl::TrackedLocaleIds & PoolImpl::trackedLocaleIds() const
      {
	if ( ! _trackedLocaleIdsPtr )
	{
	  _trackedLocaleIdsPtr.reset( new TrackedLocaleIds );

	  const base::SetTracker<LocaleSet> &	localesTracker( _requestedLocalesTracker );
	  TrackedLocaleIds &			localeIds( *_trackedLocaleIdsPtr );

	  // Add current locales+fallback except for added ones
	  for ( Locale lang: localesTracker.current() )
	  {
	    if ( localesTracker.wasAdded( lang ) )
	      continue;
	    for ( ; lang; lang = lang.fallback() )
	    { localeIds.current().insert( IdString(lang) ); }
	  }

	  // Add added locales+fallback except they are already in current
	  for ( Locale lang: localesTracker.added() )
	  {
	    for ( ; lang && localeIds.current().insert( IdString(lang) ).second; lang = lang.fallback() )
	    { localeIds.added().insert( IdString(lang) ); }
	  }

	  // Add removed locales+fallback except they are still in current
	  for ( Locale lang: localesTracker.removed() )
	  {
	    for ( ; lang && ! localeIds.current().count( IdString(lang) ); lang = lang.fallback() )
	    { localeIds.removed().insert( IdString(lang) ); }
	  }

	  // Assert that TrackedLocaleIds::current is not empty.
	  // If, so fill in LanguageCode::enCode as last resort.
	  if ( localeIds.current().empty() )
	  { localeIds.current().insert( IdString(Locale::enCode) ); }
	}
	return *_trackedLocaleIdsPtr;
      }


      static void _getLocaleDeps( const Capability & cap_r, LocaleSet & store_r )
      {
        // Collect locales from any 'namespace:language(lang)' dependency
        CapDetail detail( cap_r );
        if ( detail.kind() == CapDetail::EXPRESSION )
        {
          switch ( detail.capRel() )
          {
            case CapDetail::CAP_AND:
            case CapDetail::CAP_OR:
              // expand
              _getLocaleDeps( detail.lhs(), store_r );
              _getLocaleDeps( detail.rhs(), store_r );
              break;

            case CapDetail::CAP_NAMESPACE:
              if ( detail.lhs().id() == NAMESPACE_LANGUAGE )
              {
                store_r.insert( Locale( IdString(detail.rhs().id()) ) );
              }
              break;

            case CapDetail::REL_NONE:
            case CapDetail::CAP_WITH:
            case CapDetail::CAP_ARCH:
              break; // unwanted
          }
        }
      }

      const LocaleSet & PoolImpl::getAvailableLocales() const
      {
        if ( !_availableLocalesPtr )
        {
	  _availableLocalesPtr.reset( new LocaleSet );
	  LocaleSet & localeSet( *_availableLocalesPtr );

	  for ( const Solvable & pi : Pool::instance().solvables() )
	  {
	    for ( const Capability & cap : pi.supplements() )
	    {
	      _getLocaleDeps( cap, localeSet );
            }
	  }
        }
        return *_availableLocalesPtr;
      }

      ///////////////////////////////////////////////////////////////////

      void PoolImpl::multiversionListInit() const
      {
        _multiversionListPtr.reset( new MultiversionList );
        MultiversionList & multiversionList( *_multiversionListPtr );
	
	MultiversionList::size_type size = 0;
        for ( const std::string & spec : ZConfig::instance().multiversionSpec() )
	{
	  static const std::string prefix( "provides:" );
	  bool provides = str::hasPrefix( spec, prefix );

	  for ( Solvable solv : WhatProvides( Capability( provides ? spec.c_str() + prefix.size() : spec.c_str() ) ) )
	  {
	    if ( provides || solv.ident() == spec )
	      multiversionList.insert( solv );
	  }

	  MultiversionList::size_type nsize = multiversionList.size();
	  MIL << "Multiversion install " << spec << ": " << (nsize-size) << " matches" << endl;
	  size = nsize;
        }
      }

      void PoolImpl::multiversionSpecChanged()
      { _multiversionListPtr.reset(); }

      const PoolImpl::MultiversionList & PoolImpl::multiversionList() const
      {
	if ( ! _multiversionListPtr )
	  multiversionListInit();
	return *_multiversionListPtr;
      }

      bool PoolImpl::isMultiversion( const Solvable & solv_r ) const
      { return multiversionList().contains( solv_r ); }

      ///////////////////////////////////////////////////////////////////

      const std::set<std::string> & PoolImpl::requiredFilesystems() const
      {
	if ( ! _requiredFilesystemsPtr )
	{
	  _requiredFilesystemsPtr.reset( new std::set<std::string> );
	  std::set<std::string> & requiredFilesystems( *_requiredFilesystemsPtr );
	  str::split( base::sysconfig::read( sysconfigStoragePath() )["USED_FS_LIST"],
		      std::inserter( requiredFilesystems, requiredFilesystems.end() ) );
	}
	return *_requiredFilesystemsPtr;
      }

      /////////////////////////////////////////////////////////////////
    } // namespace detail
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////
  } // namespace sat
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
