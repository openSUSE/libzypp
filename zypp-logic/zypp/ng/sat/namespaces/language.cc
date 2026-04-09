/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "language.h"

#include <zypp-core/base/LogTools.h>
#include <zypp/ng/sat/capability.h>

#include <zypp/ng/sat/pool.h>
#include <zypp/ng/sat/solvable.h>

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zyppng::satpool::namespaces"

namespace zyppng::sat::namespaces {

  bool LanguageNamespaceProvider::isSatisfied(detail::IdType value) const
  {
    const TrackedLocaleIds & localeIds( trackedLocaleIds() );
    return localeIds.contains( IdString(value) );
  }

  void LanguageNamespaceProvider::initRequestedLocales( const LocaleSet & locales_r )
  {
    if ( _requestedLocalesTracker.setInitial( locales_r ) )
    {
      localeSetDirty( {"initRequestedLocales"} );
      MIL << "Init RequestedLocales: " << _requestedLocalesTracker << " =" << locales_r << std::endl;
    }
  }

  void LanguageNamespaceProvider::setRequestedLocales( const LocaleSet & locales_r )
  {
    if ( _requestedLocalesTracker.set( locales_r ) )
    {
      localeSetDirty( {"setRequestedLocales"} );
      MIL << "New RequestedLocales: " << _requestedLocalesTracker << " =" << locales_r << std::endl;
    }
  }

  bool LanguageNamespaceProvider::addRequestedLocale( const Locale & locale_r )
  {
    bool done = _requestedLocalesTracker.add( locale_r );
    if ( done )
    {
      localeSetDirty( {"addRequestedLocale", locale_r.code().c_str()} );
      MIL << "New RequestedLocales: " << _requestedLocalesTracker << " +" << locale_r << std::endl;
    }
    return done;
  }

  bool LanguageNamespaceProvider::eraseRequestedLocale( const Locale & locale_r )
  {
    bool done = _requestedLocalesTracker.remove( locale_r );
    if ( done )
    {
      localeSetDirty( {"eraseRequestedLocale", locale_r.code().c_str()} );
      MIL << "New RequestedLocales: " << _requestedLocalesTracker << " -" << locale_r << std::endl;
    }
    return done;
  }

  const LanguageNamespaceProvider::TrackedLocaleIds & LanguageNamespaceProvider::trackedLocaleIds() const
  {
    if ( ! _trackedLocaleIdsPtr )
    {
      _trackedLocaleIdsPtr.reset( new TrackedLocaleIds );

      const auto&	localesTracker( _requestedLocalesTracker );
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

      // bsc#1155678: We try to differ between an empty RequestedLocales
      // and one containing 'en' (explicit or as fallback). An empty RequestedLocales
      // should not even drag in recommended 'en' packages. So we no longer enforce
      // 'en' being in the set.
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

        default:
          break; // unwanted
      }
    }
  }

  const LocaleSet & LanguageNamespaceProvider::getAvailableLocales() const
  {
    if ( !_availableLocalesPtr )  {

      _availableLocalesPtr.reset( new LocaleSet );
      LocaleSet & localeSet( *_availableLocalesPtr );

      const auto solvables = zypp::makeIterable( detail::SolvableIterator( _pool->getFirstId() ), detail::SolvableIterator() );
      for ( const Solvable & pi : solvables )
      {
        for ( const Capability & cap : pi.dep_supplements() )
        {
          _getLocaleDeps( cap, localeSet );
        }
      }
    }
    return *_availableLocalesPtr;
  }

}
