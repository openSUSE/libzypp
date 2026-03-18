/*---------------------------------------------------------------------
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
#ifndef ZYPP_NG_SAT_COMPONENTS_ARCHITECTURECOMPONENT_H_INCLUDED
#define ZYPP_NG_SAT_COMPONENTS_ARCHITECTURECOMPONENT_H_INCLUDED

#include "poolcomponents.h"
#include <zypp/ng/arch.h>
#include <zypp/ng/base/serialnumber.h>
#include <functional>

namespace zyppng::sat {

  /**
   * \brief Component managing the Pool architecture.
   *
   * This component ensures that libsolv's internal architecture is synced
   * with the system or a custom provider whenever the pool content changes.
   */
  class ArchitectureComponent : public IPoolComponent {
    public:
      /** \brief Provider function returning an Arch. */
      using ArchitectureProvider = std::function<zypp::Arch()>;

      ArchitectureComponent() = default;

      /** \brief Set a custom architecture provider. */
      void setProvider( ArchitectureProvider provider )
      { _provider = std::move(provider); }

      /** \brief Get the current architecture. */
      zypp::Arch arch() const
      { return _provider ? _provider() : zypp::Arch::detectSystemArchitecture(); }

      InitStage stage() const override { return InitStage::Environment; }
      int priority() const override { return 0; }

      void prepare( Pool & pool ) override;
      void onRepoAdded( Pool & pool, detail::RepoIdType id ) override;
      void onReset( Pool & pool ) override;

    private:
      ArchitectureProvider _provider;
      SerialNumberWatcher _watcher;
  };

} // namespace zyppng::sat

#endif
