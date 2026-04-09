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
  class IPoolComponent;

  /**
   * \brief Execution stages for PoolComponents.
   * Guaranteed to be executed in this order during pool.prepare().
   */
  enum class ComponentStage : size_t {
      Environment, ///< Arches, Locales, Namespace Callbacks (The Foundation)
      Policy,      ///< Blacklists, Reboot Specs, Storage Policy
      Metadata,    ///< ID-indexed metadata stores
      Logging,     ///< Diagnostic components
      COUNT        ///< Sentinel
  };

  /**
   * \brief Base interface for modular Pool logic and services.
   *
   * IPoolComponent allows the Pool to be extended with specialized logic (like architecture
   * resolution or metadata storage) without bloating the core Pool class.
   *
   * \section lifecycle Lifecycle and Execution
   * Components are categorized by \ref ComponentStage to ensure deterministic initialization.
   * 1. **Attach**: \ref attach is called immediately when the component is added to a Pool.
   * 2. **Prepare**: \ref prepare is called during pool.prepare() in stage order. This is
   * the time to sync C++ state to the underlying libsolv structures.
   * 3. **Invalidate**: \ref onInvalidate is called when the pool state changes. A 'hard'
   * invalidation means the underlying libsolv Pool was destroyed.
   */
  class IPoolComponent {
    public:

      IPoolComponent(const IPoolComponent &) = delete;
      IPoolComponent(IPoolComponent &&) = delete;
      IPoolComponent &operator=(const IPoolComponent &) = delete;
      IPoolComponent &operator=(IPoolComponent &&) = delete;

      IPoolComponent() = default;
      virtual ~IPoolComponent() = default;
      virtual void attach  ( Pool & ) { /* does nothing by default */ }
      virtual void prepare ( Pool & ) { /* does nothing by default */ }
      virtual void onInvalidate ( Pool &, PoolInvalidation ) { }

      virtual void onRepoAdded( Pool &, detail::RepoIdType /*id*/) {}
      virtual void onRepoRemoved( Pool &, detail::RepoIdType /*id*/) {}

      virtual ComponentStage stage() const { return ComponentStage::Policy; }

      /**
       *  \brief Define fine-grained ordering within a Stage.
       *         Lower values run earlier. Default is 0.
       *  \note use \sa stage to do a basic sorting, only resort to priority if you need
       *        to run early or late in the given stage
       */
      virtual int priority() const { return 0; }
  };


  class PoolComponentSet {
    public:
      PoolComponentSet() {
        std::fill( _dirtyBuckets.begin (), _dirtyBuckets.end(), false );
      }
      PoolComponentSet(const PoolComponentSet &) = delete;
      PoolComponentSet(PoolComponentSet &&) = default;
      PoolComponentSet &operator=(const PoolComponentSet &) = delete;
      PoolComponentSet &operator=(PoolComponentSet &&) = default;

      template <typename T>
      T &assertComponent( std::unique_ptr<T> &&compPtr = {} ) {

        std::type_index idx( typeid(T) );
        if ( _components.count ( idx ))
          return *static_cast<T*>(_components[idx]->get());

        auto ptr = std::move(compPtr);
        if ( !ptr ) {
          ptr = std::make_unique<T>();
        }

        std::unique_ptr<TypeErasure> component = std::make_unique<CompContainer<T>>( std::move(ptr) ) ;
        auto iter = _components.insert( { std::move(idx), std::move(component) } );

        auto genericComp = static_cast<T *>( iter.first->second->get() );
        if constexpr ( std::is_base_of_v<IPoolComponent, T> ) {
          auto iComp = static_cast<IPoolComponent *>( genericComp );
          _lifecycleComponents[static_cast<size_t>(iComp->stage())].push_back(iComp);
          _dirtyBuckets[static_cast<size_t>(iComp->stage())] = true;
        }

        return *genericComp;
      }

      template <typename T>
      const T *findComponent() const {
        const std::type_index idx( typeid(T) );
        if ( _components.count ( idx ))
          return static_cast<T*>(_components.at(idx)->get());
        return nullptr;
      }

      void notifyPrepare ( Pool & pool ) {
        sortBuckets ();
        for( auto &bucket : _lifecycleComponents ) {
          for ( auto &comp : bucket ) {
            comp->prepare ( pool );
          }
        }
      }

      void notifyInvalidate ( Pool &pool, PoolInvalidation invalidation )  {
        sortBuckets ();
        for( auto &bucket : _lifecycleComponents ) {
          for ( auto &comp : bucket ) {
            comp->onInvalidate ( pool, invalidation );
          }
        }
      }

      void notifyRepoAdded( Pool &pool, detail::RepoIdType id ) {
        sortBuckets ();
        for( auto &bucket : _lifecycleComponents ) {
          for ( auto &comp : bucket ) {
            comp->onRepoAdded ( pool, id  );
          }
        }
      }

      void notifyRepoRemoved( Pool &pool, detail::RepoIdType id ) {
        sortBuckets ();
        for( auto &bucket : _lifecycleComponents ) {
          for ( auto &comp : bucket ) {
            comp->onRepoRemoved ( pool, id  );
          }
        }
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

          CompContainer( std::unique_ptr<T> component ) : _ptr ( std::move(component) ) {}
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

      void sortBuckets() {
        for ( size_t i = 0; i < static_cast<size_t>(ComponentStage::COUNT); i++ ) {
          if ( _dirtyBuckets[i] ) {
            auto &bucket = _lifecycleComponents[i];
            _dirtyBuckets[i] = false;
            std::stable_sort( bucket.begin (), bucket.end(), [](const auto &a, const auto &b ) {
              return a->priority() < b->priority();
            });
          }
        }
      }

    private:
      std::unordered_map< std::type_index, std::unique_ptr<TypeErasure> > _components;
      std::array<std::vector<IPoolComponent*>, static_cast<size_t>(ComponentStage::COUNT)> _lifecycleComponents;
      std::array<bool, static_cast<size_t>(ComponentStage::COUNT)> _dirtyBuckets = {false,};
  };
}



#endif
