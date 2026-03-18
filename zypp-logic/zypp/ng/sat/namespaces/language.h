/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPP_NG_SAT_NAMESPACES_LANGUAGE_H_INCLUDED
#define ZYPP_NG_SAT_NAMESPACES_LANGUAGE_H_INCLUDED

#include <zypp/ng/base/settracker.h>
#include <zypp/ng/sat/namespaces/namespaceprovider.h>
#include <zypp/ng/locale.h>
#include <zypp/ng/idstring.h>

#include <memory>

namespace zyppng::sat::namespaces {

  /**
   * \brief Provider for NAMESPACE_LANGUAGE.
   * Matches against requested locales and their fallbacks.
   */
  class LanguageNamespaceProvider : public NamespaceProvider {
    public:

      LanguageNamespaceProvider() = default;
      bool isSatisfied( detail::IdType value ) const override;

      /** Start tracking changes based on this \a locales_r.
       * Usually called on TargetInit.
       */
      void initRequestedLocales( const LocaleSet & locales_r );

      /** Added since last initRequestedLocales. */
      const LocaleSet & getAddedRequestedLocales() const
      { return _requestedLocalesTracker.added(); }

      /** Removed since last initRequestedLocales. */
      const LocaleSet & getRemovedRequestedLocales() const
      { return _requestedLocalesTracker.removed(); }

      /** Current set of requested Locales. */
      const LocaleSet & getRequestedLocales() const
      { return _requestedLocalesTracker.current(); }

      bool isRequestedLocale( const Locale & locale_r ) const
      { return _requestedLocalesTracker.contains( locale_r ); }

      /** User change (tracked). */
      void setRequestedLocales( const LocaleSet & locales_r );
      /** User change (tracked). */
      bool addRequestedLocale( const Locale & locale_r );
      /** User change (tracked). */
      bool eraseRequestedLocale( const Locale & locale_r );

      /** All Locales occurring in any repo. */
      const LocaleSet & getAvailableLocales() const;

      void onReset( Pool & pool ) override;

      bool isAvailableLocale( const Locale & locale_r ) const
      {
        const LocaleSet & avl( getAvailableLocales() );
        LocaleSet::const_iterator it( avl.find( locale_r ) );
        return it != avl.end();
      }

      using TrackedLocaleIds = base::SetTracker<IdStringSet> ;

      /** Expanded _requestedLocalesTracker for solver.*/
      const TrackedLocaleIds & trackedLocaleIds() const;

    private:

      void localeSetDirty( std::initializer_list<std::string_view> reasons ) {
        _trackedLocaleIdsPtr.reset();	// requested locales changed
        notifyDirty( PoolInvalidation::Dependency, std::move(reasons) ); // invalidate dependency/namespace related indices
      }

      base::SetTracker<LocaleSet> _requestedLocalesTracker;
      mutable std::unique_ptr<TrackedLocaleIds> _trackedLocaleIdsPtr; // mutable because of lazy init
      mutable std::unique_ptr<LocaleSet>        _availableLocalesPtr; // mutable because of lazy init

  };

}

#endif
