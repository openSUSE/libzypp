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
#ifndef ZYPP_NG_SAT_NAMESPACES_MODALIAS_H_INCLUDED
#define ZYPP_NG_SAT_NAMESPACES_MODALIAS_H_INCLUDED

#include <zypp/ng/sat/namespaces/namespaceprovider.h>
#include <string>
#include <functional>

namespace zyppng::sat::namespaces {

/**
   * \brief Provider for NAMESPACE_MODALIAS.
   * Checks against system modaliases (e.g., PCI/USB IDs).
   */
  class ModaliasNamespaceProvider : public NamespaceProvider {
    public:
      ModaliasNamespaceProvider() = default;
      bool isSatisfied( detail::IdType value ) const override;

      /**
       * \brief Callback to perform the actual modalias query.
       * Decouples the provider from the concrete hardware detection backend.
       */
      using ModaliasQuery = std::function<bool(const std::string &)>;
      void setQueryCallback( ModaliasQuery query ) { _query = std::move(query); }

    private:
      ModaliasQuery _query;
  };

}

#endif
