/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::Locale into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <unordered_set>
#include <zypp/Locale.h>

export module zyppng:locale;

// Locale, LanguageCode, and CountryCode all derive from IdStringType<T>.
// Their free-function operators are GMF-attached and not reachable via ADL
// across the module boundary.
// Re-exporting :idstringtype makes them reachable for any TU that imports :locale.
export import :idstringtype;

export namespace zyppng {
  using ::zypp::Locale;
  using LocaleSet = std::unordered_set<Locale>;
} // namespace zyppng
