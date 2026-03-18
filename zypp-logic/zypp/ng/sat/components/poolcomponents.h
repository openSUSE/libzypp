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
#ifndef ZYPP_NG_SAT_COMPONENTS_POOLCOMPONENTS_H_INCLUDED
#define ZYPP_NG_SAT_COMPONENTS_POOLCOMPONENTS_H_INCLUDED

#include <algorithm>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <array>
#include <vector>
#include <zypp/ng/sat/poolconstants.h>

namespace zyppng::sat {

  class Pool;
  class PreparedPool;

  /**
   * \brief Execution stages for IPoolComponent (pre-index phase).
   * Guaranteed to be executed in this order during pool.prepare(),
   * before the whatprovides index is built.
   */
  enum class InitStage : size_t {
      Environment, ///< Arches, Locales, Namespace Callbacks (The Foundation)
      COUNT        ///< Sentinel
  };

  /**
   * \brief Execution stages for IPreparedPoolComponent (post-index phase).
   * Guaranteed to be executed in this order during pool.prepare(),
   * after the whatprovides index is built.
   */
  enum class PreparedStage : size_t {
      Policy,   ///< Blacklists, Reboot Specs, Storage Policy
      Metadata, ///< ID-indexed metadata stores
      Logging,  ///< Diagnostic components
      COUNT     ///< Sentinel
  };

  namespace detail {
    /**
     * \brief Shared base for IPoolComponent and IPreparedPoolComponent.
     *
     * Not intended for direct inheritance outside of those two interfaces.
     * Provides the lifecycle callbacks common to both pre- and post-index phases.
     */
    class IBasicPoolComponent {
    public:
      /** Probe external state. May call pool.setDirty(). Called once per
       *  prepare() invocation, before any component prepare work starts.
       *  This is the ONLY point during the prepare sequence where calling
       *  setDirty() is legal (asserted in debug builds). */
      virtual void checkDirty   ( Pool & )                   {}
      /** React to pool invalidation — clear internal caches. */
      virtual void onInvalidate ( Pool &, PoolInvalidation ) {}
      virtual void onRepoAdded  ( Pool &, RepoIdType )       {}
      virtual void onRepoRemoved( Pool &, RepoIdType )       {}
      virtual void onReset      ( Pool & )                   {}

      virtual ~IBasicPoolComponent() = default;
      IBasicPoolComponent() = default;
      IBasicPoolComponent(const IBasicPoolComponent &) = delete;
      IBasicPoolComponent(IBasicPoolComponent &&)      = delete;
      IBasicPoolComponent &operator=(const IBasicPoolComponent &) = delete;
      IBasicPoolComponent &operator=(IBasicPoolComponent &&)      = delete;
    };
  } // namespace detail

  /**
   * \brief Interface for components that run BEFORE the whatprovides index is built.
   *
   * Examples:
   * - ArchitectureComponent — sets pool arch via libsolv arch functions
   * - NamespaceComponent    — registers the libsolv namespace callback on attach()
   *
   * Has stage()/priority() for deterministic pre-index ordering.
   * \note Calling setDirty() inside prepare() is a bug, asserted in debug builds.
   */
  class IPoolComponent : public detail::IBasicPoolComponent {
  public:
    virtual void attach ( Pool & ) {}
    virtual void prepare( Pool & ) {}
    virtual InitStage stage()    const { return InitStage::Environment; }
    /**
     * \brief Fine-grained ordering within a stage. Lower values run earlier. Default is 0.
     * \note Prefer stage() for coarse ordering; use priority() only when ordering within
     *       a stage matters.
     */
    virtual int  priority() const { return 0; }
  };

  /**
   * \brief Interface for components that run AFTER the whatprovides index is built.
   *
   * Examples:
   * - PackagePolicyComponent — expands provides tokens into EvaluatedSolvableSpec caches
   *
   * Has stage()/priority() for deterministic post-index ordering.
   * \note Calling setDirty() inside prepare() is a bug, asserted in debug builds.
   */
  class IPreparedPoolComponent : public detail::IBasicPoolComponent {
  public:
    virtual void attach ( Pool & )         {}
    virtual void prepare( PreparedPool & ) {}
    virtual PreparedStage stage()    const { return PreparedStage::Policy; }
    /**
     * \brief Fine-grained ordering within a stage. Lower values run earlier. Default is 0.
     * \note Prefer stage() for coarse ordering; use priority() only when ordering within
     *       a stage matters.
     */
    virtual int           priority() const { return 0; }
  };


  /**
   * \brief Registry and dispatcher for all pool components.
   *
   * Maintains three internal structures:
   * - A unified flat list of IBasicPoolComponent* for checkDirty/onInvalidate/onRepo* callbacks.
   * - Stage-bucketed lists of IPoolComponent* for pre-index prepare dispatch.
   * - Stage-bucketed lists of IPreparedPoolComponent* for post-index prepare dispatch.
   *
   * Dual-phase components (inheriting both IPoolComponent and IPreparedPoolComponent)
   * are explicitly forbidden by policy — split into two cooperating components instead.
   */
  class PoolComponentSet {
    public:
      PoolComponentSet() = default;
      PoolComponentSet(const PoolComponentSet &) = delete;
      PoolComponentSet(PoolComponentSet &&) = default;
      PoolComponentSet &operator=(const PoolComponentSet &) = delete;
      PoolComponentSet &operator=(PoolComponentSet &&) = default;

      template <typename T>
      T &assertComponent( std::unique_ptr<T> &&compPtr = {} ) {

        std::type_index idx( typeid(T) );
        if ( _components.count( idx ) )
          return *static_cast<T*>( _components[idx]->get() );

        auto ptr = std::move(compPtr);
        if ( !ptr )
          ptr = std::make_unique<T>();

        std::unique_ptr<TypeErasure> component = std::make_unique<CompContainer<T>>( std::move(ptr) );
        auto iter = _components.insert( { std::move(idx), std::move(component) } );
        auto genericComp = static_cast<T *>( iter.first->second->get() );

        // Unified basic-callbacks list — always registered when the type derives from IBasicPoolComponent.
        if constexpr ( std::is_base_of_v<detail::IBasicPoolComponent, T> )
          _basicComponents.push_back( static_cast<detail::IBasicPoolComponent*>( genericComp ) );

        // Pre-index component list (stage-bucketed, priority-sorted within bucket).
        if constexpr ( std::is_base_of_v<IPoolComponent, T> ) {
          auto iComp = static_cast<IPoolComponent*>( genericComp );
          _initComponents[static_cast<size_t>( iComp->stage() )].push_back( iComp );
          _initDirtyBuckets[static_cast<size_t>( iComp->stage() )] = true;
        }

        // Post-index component list (stage-bucketed, priority-sorted within bucket).
        if constexpr ( std::is_base_of_v<IPreparedPoolComponent, T> ) {
          auto iComp = static_cast<IPreparedPoolComponent*>( genericComp );
          _preparedComponents[static_cast<size_t>( iComp->stage() )].push_back( iComp );
          _preparedDirtyBuckets[static_cast<size_t>( iComp->stage() )] = true;
        }

        return *genericComp;
      }

      template <typename T>
      const T *findComponent() const {
        const std::type_index idx( typeid(T) );
        if ( _components.count( idx ) )
          return static_cast<T*>( _components.at(idx)->get() );
        return nullptr;
      }

      /** Pass 1 of prepare(): probe external state. setDirty() is legal here. */
      void notifyCheckDirty( Pool & pool ) {
        for ( auto *comp : _basicComponents )
          comp->checkDirty( pool );
      }

      /** Pass 2 of prepare(): pre-index component work (stage/priority order). */
      void notifyPrepare( Pool & pool ) {
        sortInitBuckets();
        for ( auto & bucket : _initComponents )
          for ( auto *comp : bucket )
            comp->prepare( pool );
      }

      /** Pass 3 of prepare(): post-index component work (stage/priority order). */
      void notifyPrepareWithIndex( PreparedPool & pp ) {
        sortPreparedBuckets();
        for ( auto & bucket : _preparedComponents )
          for ( auto *comp : bucket )
            comp->prepare( pp );
      }

      void notifyInvalidate( Pool & pool, PoolInvalidation invalidation ) {
        for ( auto *comp : _basicComponents )
          comp->onInvalidate( pool, invalidation );
      }

      void notifyRepoAdded( Pool & pool, detail::RepoIdType id ) {
        for ( auto *comp : _basicComponents )
          comp->onRepoAdded( pool, id );
      }

      void notifyRepoRemoved( Pool & pool, detail::RepoIdType id ) {
        for ( auto *comp : _basicComponents )
          comp->onRepoRemoved( pool, id );
      }

      void notifyReset( Pool & pool ) {
        for ( auto *comp : _basicComponents )
          comp->onReset( pool );
      }

    private:
      struct TypeErasure {
          TypeErasure() = default;
          virtual ~TypeErasure() = default;
          virtual void * get() = 0;
          TypeErasure(const TypeErasure &) = delete;
          TypeErasure(TypeErasure &&) = delete;
          TypeErasure &operator=(const TypeErasure &) = delete;
          TypeErasure &operator=(TypeErasure &&) = delete;
      };

      template <typename T>
      struct CompContainer : public TypeErasure {
          CompContainer( std::unique_ptr<T> component ) : _ptr( std::move(component) ) {}
          ~CompContainer() override = default;
          CompContainer(const CompContainer &) = delete;
          CompContainer(CompContainer &&) = delete;
          CompContainer &operator=(const CompContainer &) = delete;
          CompContainer &operator=(CompContainer &&) = delete;

          std::unique_ptr<T> _ptr;

          std::type_index typeIndex() const {
            return typeid(T);
          }

          void *get() override {
            return _ptr.get();
          }
      };

      void sortInitBuckets() {
        for ( size_t i = 0; i < static_cast<size_t>(InitStage::COUNT); i++ ) {
          if ( _initDirtyBuckets[i] ) {
            auto & bucket = _initComponents[i];
            _initDirtyBuckets[i] = false;
            std::stable_sort( bucket.begin(), bucket.end(), []( const auto &a, const auto &b ) {
              return a->priority() < b->priority();
            });
          }
        }
      }

      void sortPreparedBuckets() {
        for ( size_t i = 0; i < static_cast<size_t>(PreparedStage::COUNT); i++ ) {
          if ( _preparedDirtyBuckets[i] ) {
            auto & bucket = _preparedComponents[i];
            _preparedDirtyBuckets[i] = false;
            std::stable_sort( bucket.begin(), bucket.end(), []( const auto &a, const auto &b ) {
              return a->priority() < b->priority();
            });
          }
        }
      }

    private:
      std::unordered_map<std::type_index, std::unique_ptr<TypeErasure>> _components;

      /// Unified list for checkDirty / onInvalidate / onRepoAdded / onRepoRemoved
      std::vector<detail::IBasicPoolComponent*> _basicComponents;

      /// Pre-index components — stage-bucketed, priority-sorted within bucket
      std::array<std::vector<IPoolComponent*>, static_cast<size_t>(InitStage::COUNT)> _initComponents;
      std::array<bool, static_cast<size_t>(InitStage::COUNT)> _initDirtyBuckets = {false,};

      /// Post-index components — stage-bucketed, priority-sorted within bucket
      std::array<std::vector<IPreparedPoolComponent*>, static_cast<size_t>(PreparedStage::COUNT)> _preparedComponents;
      std::array<bool, static_cast<size_t>(PreparedStage::COUNT)> _preparedDirtyBuckets = {false,};
  };

} // namespace zyppng::sat

#endif
