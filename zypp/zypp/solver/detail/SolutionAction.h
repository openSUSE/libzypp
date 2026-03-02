/*
 *
 * Easy-to use interface to the ZYPP dependency resolver
 *
 * Author: Stefan Hundhammer <sh@suse.de>
 *
 **/

#ifndef ZYPP_SOLVER_DETAIL_SOLUTIONACTION_H
#define ZYPP_SOLVER_DETAIL_SOLUTIONACTION_H
#ifndef ZYPP_USE_RESOLVER_INTERNALS
#error Do not directly include this file!
#else

#include <list>
#include <string>

#include <zypp/base/ReferenceCounted.h>
#include <zypp-core/base/PtrTypes.h>

#include <zypp/PoolItem.h>

/////////////////////////////////////////////////////////////////////////
namespace zypp
{ ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  namespace solver
  { /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
    namespace detail
    { ///////////////////////////////////////////////////////////////////

      class Resolver;

      DEFINE_PTR_TYPE(SolverQueueItem);

      DEFINE_PTR_TYPE(SolutionAction);
      typedef std::list<SolutionAction_Ptr> SolutionActionList;

      /**
       * Abstract base class for one action of a problem solution.
       **/
      class SolutionAction : public base::ReferenceCounted
      {
      protected:
        typedef Resolver ResolverInternal;
        SolutionAction();

      public:
        virtual ~SolutionAction();

        virtual std::ostream & dumpOn( std::ostream & str ) const;

        /**
         * Execute this action.
         * Returns 'true' on success, 'false' on error.
         **/
        virtual bool execute( ResolverInternal & resolver ) const = 0;

      public:
        /** The PoolItem the solution refers to (if any). */
        virtual PoolItem item() const;

        /** The solution contains only 'do not install patch:' actions. */
        virtual bool skipsPatchesOnly() const;
      };

      inline std::ostream & operator<<( std::ostream & str, const SolutionAction & action )
      { return action.dumpOn( str ); }

      std::ostream & operator<<( std::ostream & str, const SolutionActionList & actionlist );


      /**
       * A problem solution action that performs a transaction
       * (installs, removes, keep ...)  one resolvable
       * (package, patch, pattern, product).
       **/
      typedef enum
      {
        KEEP,
        INSTALL,
        REMOVE,
        UNLOCK,
        LOCK,
        REMOVE_EXTRA_REQUIRE,
        REMOVE_EXTRA_CONFLICT,
        ADD_SOLVE_QUEUE_ITEM,
        REMOVE_SOLVE_QUEUE_ITEM,
      } TransactionKind;

      class TransactionSolutionAction: public SolutionAction
      {
      public:
        TransactionSolutionAction( PoolItem item, TransactionKind action )
        : SolutionAction()
        , _action( action )
        , _item( item )
        {}

        TransactionSolutionAction( Capability capability, TransactionKind action )
        : SolutionAction()
        , _action( action )
        , _capability( capability )
        {}

        TransactionSolutionAction( SolverQueueItem_Ptr item, TransactionKind action )
        : SolutionAction()
        , _action( action )
        , _solverQueueItem( item )
        {}

        TransactionSolutionAction( TransactionKind action )
        : SolutionAction()
        , _action( action )
        {}

        std::ostream & dumpOn( std::ostream & str ) const override;

        bool execute( ResolverInternal & resolver ) const override;

      public:
        PoolItem item() const override
        { return _item; }

        bool skipsPatchesOnly() const override;

      protected:
        const TransactionKind _action;

        PoolItem _item;
        Capability _capability;
        SolverQueueItem_Ptr _solverQueueItem;
      };


      /**
       * Type of ignoring; currently only WEAK
       **/
      typedef enum
      {
        WEAK
      } InjectSolutionKind;

      /**
       * A problem solution action that injects an artificial "provides" to
       * the pool to satisfy open requirements or remove the conflict of
       * concerning resolvable
       *
       * This is typically used by "ignore" (user override) solutions.
       **/
      class InjectSolutionAction: public SolutionAction
      {
      public:
        InjectSolutionAction( PoolItem item, const InjectSolutionKind & kind )
        : SolutionAction()
        , _kind( kind )
        , _item( item )
        {}

        std::ostream & dumpOn( std::ostream & str ) const override;

        bool execute( ResolverInternal & resolver ) const override;

      public:
        PoolItem item() const  override
        { return _item; }

      protected:
        const InjectSolutionKind _kind;

        PoolItem _item;
      };


      ///////////////////////////////////////////////////////////////////
    };// namespace detail
    /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
  };// namespace solver
  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
};// namespace zypp
/////////////////////////////////////////////////////////////////////////
#endif // ZYPP_USE_RESOLVER_INTERNALS
#endif // ZYPP_SOLVER_DETAIL_SOLUTIONACTION_H

