/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ui/Selectable.h
 *
*/
#ifndef ZYPP_UI_SELECTABLE_H
#define ZYPP_UI_SELECTABLE_H

#include <iosfwd>
#include <utility>

#include <zypp/base/ReferenceCounted.h>
#include <zypp/base/NonCopyable.h>
#include <zypp/base/PtrTypes.h>
#include <zypp/base/Iterator.h>

#include <zypp/ui/SelectableTraits.h>
#include <zypp/ui/Status.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  namespace ui
  { /////////////////////////////////////////////////////////////////

    DEFINE_PTR_TYPE(Selectable);

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : Selectable
    //
    /** Collects PoolItems of same kind and name.
     *
     * Selectable is a status wrapper. The ui::Status is calculated
     * from (and transated to) \ref PoolItems individual \ref ResStatus
     * values.
     *
     * Available objects are sorted according the solver policies, 'best'
     * packages first (e.g. by repository priority, then Arch, then Edition).
     *
     * Installed objects are sorted according the installation date, newer install
     * time first.
    */
    class ZYPP_API Selectable : public base::ReferenceCounted, private base::NonCopyable
    {
      friend std::ostream & operator<<( std::ostream & str, const Selectable & obj );
      friend std::ostream & dumpOn( std::ostream & str, const Selectable & obj );

    public:
      using Ptr = intrusive_ptr<Selectable>;
      using constPtr = intrusive_ptr<const Selectable>;

      /** Iterates over ResObject::constPtr */
      using available_iterator = SelectableTraits::available_iterator;
      using available_size_type = SelectableTraits::available_size_type;

      using installed_iterator = SelectableTraits::installed_iterator;
      using installed_size_type = SelectableTraits::installed_size_type;

      using picklist_iterator = SelectableTraits::picklist_iterator;
      using picklist_size_type = SelectableTraits::picklist_size_type;

    public:
      /** \name Static ctor substitues picking the item from the pool.
       * \code
       * Selectable::Ptr item;
       * item = Selectable::get( "amarok );                  // package amamrok
       * item = Selectable::get( ResKind::patch, "amarok );  // patch amamrok
       * item = Selectable::get( IdString( "patch:amarok" ); // patch amamrok
       * \endcode
      */
      //@{
      /** Get the \ref Selctable */
      static Ptr get( const pool::ByIdent & ident_r );

      /** Get the \ref Selctable by its \c sat-identifier. */
      static Ptr get( IdString ident_r )
      { return get( pool::ByIdent( ident_r ) ); }

      /** Get the \ref Selctable by \c kind and \c name. */
      static Ptr get( ResKind kind_r, const std::string & name_r )
      { return get( pool::ByIdent( std::move(kind_r), name_r ) ); }

      /** Get the \c Package \ref Selctable by \c name. */
      static Ptr get( const std::string & name_r )
      { return get( pool::ByIdent( ResKind::package, name_r ) ); }

      /** Get the \ref Selctable containing a specific \ref sat::Solvable. */
      static Ptr get( const sat::Solvable & solv_r )
      { return get( pool::ByIdent( solv_r ) ); }

      /** Get the \ref Selctable containing a specific \ref ResObject. */
      static Ptr get( const ResObject::constPtr & resolvable_r )
      { return resolvable_r ? get( resolvable_r->satSolvable() ) : Ptr(); }

      /** Get the \ref Selctable containing a specific \ref PoolItem. */
      static Ptr get( const PoolItem & pi_r )
      { return get( pi_r.satSolvable() ); }
      //@}

    public:
      /** The identifier.
       * This is the solvables \ref name, \b except for packages and
       * source packes, prefixed by its \ref kind.
       * \see \ref sat::Solvable.
       */
      IdString ident() const;

      /** The ResObjects kind. */
      ResKind kind() const;

      /** The ResObjects name.  */
      const std::string & name() const;

      /** The last Installed object. */
      PoolItem installedObj() const;

      /** The 'best' or 'most interesting' among all available objects.
       * One that is, or is likely to be, chosen for installation, unless
       * it violated any solver policy (see \ref updateCandidateObj).
       * \note Might return a blacklisted item if explicitly set by \ref setCandidate
       * or nothing else available.
       */
      PoolItem candidateObj() const;

      /** The best candidate provided by a specific \ref Repository, if there is one.
       * In contrary to \ref candidateObj, this may return no item even if
       * there are available objects. This simply means the \ref Repository
       * does not provide this object.
       * \note Avoids to return blacklisted items.
       */
      PoolItem candidateObjFrom( Repository repo_r ) const;

      /** The best candidate for update, if there is one.
       * In contrary to \ref candidateObj, this may return no item even if
       * there are available objects. This simply means the best object is
       * already installed, and all available objects violate at least one
       * update policy. Or all available objects are blacklisted (e.g. a PTF)
       * in which there does not need to be an installed item either.
       * \note Avoids to return blacklisted items.
       */
      PoolItem updateCandidateObj() const;

      /** Simply the highest available version, ignoring priorities and policies.
       * It's doubtful whether solely looking at the version makes a good
       * candidate, but apps ask for it. Beware that different vendors may
       * use different (uncomparable) version schemata.
       * \note Avoids to return blacklisted items.
       */
      PoolItem highestAvailableVersionObj() const;

      /** Whether this \ref ident is flagged as AutoInstalled. */
      bool identIsAutoInstalled() const;

      /** \c True if \a rhs is installed and one with the same content is available.
       * Basically the same name, edition, arch, vendor and buildtime.
       * \see \ref sat::Solvable::identical
       */
      bool identicalAvailable( const PoolItem & rhs ) const;

     /** \c True if \a rhs has the same content as an installed one.
       * Basically the same name, edition, arch, vendor and buildtime.
       * \see \ref sat::Solvable::identical
       */
      bool identicalInstalled( const PoolItem & rhs ) const;

      /** \c True if the \ref candidateObj is installed (same content).
       * \see \ref identicalInstalled.
       */
      bool identicalInstalledCandidate() const
      { return identicalInstalled( candidateObj() ); }

      /** \c True if the \ref updateCandidateObj is installed (same content).
       * \see \ref identicalInstalled.
       */
      bool identicalInstalledUpdateCandidate() const
      { return identicalInstalled( updateCandidateObj() ); }

      /** Return an available Object with the same content as \c rhs.
       * Basically the same name, edition, arch, vendor and buildtime.
       * \see \ref sat::Solvable::identical
       */
      PoolItem identicalAvailableObj( const PoolItem & rhs ) const;

     /** \Return an installed Object with the same content as \c rhs.
       * Basically the same name, edition, arch, vendor and buildtime.
       * \see \ref sat::Solvable::identical
       */
      PoolItem identicalInstalledObj( const PoolItem & rhs ) const;

      /** Return the \ref installedObj resolvable casted to a specific kind.
       * \code
       *   Selectable mySelectable;
       *   Package::constPtr p( mySelectable.installedAsKind<Package>() );
       * \endcode
      */
      template<class TRes>
      typename ResTraits<TRes>::constPtrType installedAsKind() const
      { return asKind<TRes>( candidateObj() ); }

      /** Return the \ref candidateObj resolvable casted to a specific kind.
       * \code
       *   Selectable mySelectable;
       *   Package::constPtr p( mySelectable.candidateAsKind<Package>() );
       * \endcode
      */
      template<class TRes>
      typename ResTraits<TRes>::constPtrType candidateAsKind() const
      { return asKind<TRes>( candidateObj() ); }

      /** Set a candidate (out of available objects).
       * \return The new candidate, or NULL if choice was invalid
       * (NULL or not among availableObjs). An invalid choice
       * selects the default candidate.
       * In case the causer is not \c ResStatus::USER the operation
       * may also fail if there are insufficient permissions to change
       * a transacting candidate.
       */
      PoolItem setCandidate( const PoolItem & newCandidate_r, ResStatus::TransactByValue causer_r = ResStatus::USER );
      /** \overload */
      PoolItem setCandidate( const ResObject::constPtr& newCandidate_r, ResStatus::TransactByValue causer_r = ResStatus::USER );

      /** Arrange the specified candidate (out of available objects) to be on system after commit.
       * If the specified candidate is not already installed (\ref identicalInstalled),
       * and the \a causer_r has sufficient permission, then \a newCandidate_r is set as the new
       * candidate (\ref setCandidate) and selected for installation.
       * \returns \c True if \a newCandidate_r is already installed or successfully selected for installation.
       */
      bool setOnSystem( const PoolItem & newCandidate_r, ResStatus::TransactByValue causer_r = ResStatus::USER );

      /** An object you could use as pars pro toto.
       *
       * \return the \ref candidateObj, or ,if no available objects
       * exist, the \ref installedObj.
       */
      PoolItem theObj() const;

      /** The name(s) of the successor package if \ref vendorSupport is \ref VendorSupportSuperseded.
       * It returns the value for the newest candidate.
       * \see \ref Package::supersededBy
       */
      std::vector<std::string> supersededBy() const;

      ////////////////////////////////////////////////////////////////////////

      /** \name Available objects iterators.
       * Oredered according to solver policy. 'Best' first.
      */
      //@{
      bool availableEmpty() const;
      available_size_type availableSize() const;
      available_iterator availableBegin() const;
      available_iterator availableEnd() const;
      inline Iterable<available_iterator>  available() const
      { return makeIterable( availableBegin(), availableEnd() ); }
      //@}

      ////////////////////////////////////////////////////////////////////////

      /** \name Installed objects iterators.
       * Ordered by install time. Latest first.
      */
      //@{
      bool installedEmpty() const;
      installed_size_type installedSize() const;
      installed_iterator installedBegin() const;
      installed_iterator installedEnd() const;
      inline Iterable<installed_iterator> installed() const
      { return makeIterable( installedBegin(), installedEnd() ); }
      //@}

      ////////////////////////////////////////////////////////////////////////

      /** \name picklist iterators.
       * This is basically the available items list prepended by those
       * installed items, that are nolonger \ref identicalAvailable.
       */
      //@{
      bool picklistEmpty() const;
      picklist_size_type picklistSize() const;
      picklist_iterator picklistBegin() const;
      picklist_iterator picklistEnd() const;
      inline Iterable<picklist_iterator> picklist() const
      { return makeIterable( picklistBegin(), picklistEnd() ); }

      /** Returned by \ref picklistPos if the Item does not belong to the picklist. */
      static constexpr const picklist_size_type picklistNoPos = picklist_size_type(-1);

      /** Return the position of \a pi_r in the piclist or \ref picklistNoPos.
       * \note \ref picklistNoPos is returned if you pass an installed Poolitem,
       * whci has an \ref identicalAvailableObj.
       */
      picklist_size_type picklistPos( const PoolItem & pi_r ) const;

      /** \overload taking a sat::Solvable */
      picklist_size_type picklistPos( const sat::Solvable & solv_r ) const;
      //@}

      ////////////////////////////////////////////////////////////////////////

    public:
      /** \name Query for objects within this Selectable.
      */
      //@{
      /** True if either installed or candidate object is present */
      bool hasObject() const
      { return (! installedEmpty()) || candidateObj(); }

      /** True if installed object is present. */
      bool hasInstalledObj() const
      { return ! installedEmpty(); }

      /** True if candidate object is present. */
      bool hasCandidateObj() const
      { return bool(candidateObj()); }

      /** True if installed and candidate object is present */
      bool hasBothObjects() const
      { return (! installedEmpty()) && candidateObj(); }

      /** True if installed object is present but no candidate. */
      bool hasInstalledObjOnly() const
      { return (! installedEmpty()) && ! candidateObj(); }

      /** True if candidate object is present but no installed. */
      bool hasCandidateObjOnly() const
      { return ( installedEmpty() ) && candidateObj(); }


       /** True if this Selectable contains available blacklisted items (retracted,ptf,...). */
      bool hasBlacklisted() const;

      /** True if this Selectable contains an installed blacklisted item (retracted,ptf,...). */
      bool hasBlacklistedInstalled() const;

      /** True if this Selectable contains available retracted items. */
      bool hasRetracted() const;

      /** True if this Selectable contains an installed retracted item. */
      bool hasRetractedInstalled() const;

      /** True if this Selectable contains available ptf items. */
      bool hasPtf() const;

      /** True if this Selectable contains an installed ptf item. */
      bool hasPtfInstalled() const;
      //@}

      /**
       * True if this package has no replacement from
       * the available repositories
       */
      bool isUnmaintained() const;

      /** \name Multiversion install.
       *
       * Using \ref pickInstall or \ref pickDelete with non-multiversionInstall items
       * is possible, but additional constraints will apply. E.g. selecting one item for
       * install will deselect any other.
       */
      //@{
      /** Whether at least one of the available packages has multiversionInstall set.
       * \see \ref Solvable::multiversionInstall
       * \see also \ref ZConfig::multiversion.
       */
      bool multiversionInstall() const;

      /** Select a specific available item for installation.
       */
      bool pickInstall( const PoolItem & pi_r, ResStatus::TransactByValue causer_r = ResStatus::USER, bool yesno_r = true );

      /** Deselect a specific available item from installation.
      */
      bool pickNoInstall( const PoolItem & pi_r, ResStatus::TransactByValue causer_r = ResStatus::USER )
      { return pickInstall( pi_r, causer_r, false ); }

      /** Select a specific installed item for deletion.
       */
      bool pickDelete( const PoolItem & pi_r, ResStatus::TransactByValue causer_r = ResStatus::USER, bool yesno_r = true );

      /** Deselect a specific installed item from deletion.
       */
      bool pickNoDelete( const PoolItem & pi_r, ResStatus::TransactByValue causer_r = ResStatus::USER )
      { return pickDelete( pi_r, causer_r, false ); }

      /** Compute the \ref ui::Status for an individual PoolItem.
       * This just takes into account the item and any identical
       * installed (or available) one.
       * \code
       *   Assume there are 3 identical 'foo-1.1 (vendor A)' items,
       *   one 'foo-2.1 (vendor A)' and one ''foo-1.1 (vendor B)':
       *
       *   installed: . foo-1.1 (vendor A)          -> S_KeepInstalled
       *   available:   foo-2.1 (vendor A) (repo 1) -> S_NoInst
       *              . foo-1.1 (vendor A) (repo 1) -> S_KeepInstalled
       *              . foo-1.1 (vendor A) (repo 2) -> S_KeepInstalled
       *                foo-1.1 (vendor B) (repo 3) -> S_NoInst
       *
       *   After 'foo-1.1 (vendor A) (repo 1)' was selected to be installed:
       *
       *   installed: . foo-1.1 (vendor A)          -> S_Update
       *   available:   foo-2.1 (vendor A) (repo 1) -> S_NoInst
       *              I foo-1.1 (vendor A) (repo 1) -> S_Update
       *              . foo-1.1 (vendor A) (repo 2) -> S_KeepInstalled
       *                foo-1.1 (vendor B) (repo 3) -> S_NoInst
       * \endcode
       * \see \ref sat::Solvable::identical
       */
      Status pickStatus( const PoolItem & pi_r ) const;

      /** Assign a new status to a specific item. */
      bool setPickStatus( const PoolItem & pi_r, Status state_r, ResStatus::TransactByValue causer_r = ResStatus::USER );
      //@}

      /** \name Classification of available patches (pseudo installed items).
       * A patch is either \c not \c relevant, \c satisfied or \c broken.
       * The same applies to other pseudo installed kinds.
       * \see \ref traits::isPseudoInstalled
       */
      //@{
      /** Returns true for packages, because packages are not
       * classified by the solver.
       */
      bool isUndetermined() const;

      /** Returns true if the patch is relevant which means that at least
       *  one package of the patch is installed.
       */
      bool isRelevant() const;

      /** Whether a relevant patchs requirements are met. */
      bool isSatisfied() const;

      /** Whether a relevant patchs requirements are broken. */
      bool isBroken() const;

      /** This includes \c unlocked broken patches, as well as those already
       * selected to be installed. This is because already selected
       * patches will be classified as \c satisfied. \c Locked but broken
       * patches will be classified as \ref isUnwanted.
       */
      bool isNeeded() const;

      /** Broken (needed) but locked patches. */
       bool isUnwanted() const;
      //@}

     public:
      /** \name Query and maip objects fate in case of commit.
       */
      //@{
      enum Fate {
        TO_DELETE  = -1,
        UNMODIFIED = 0,
        TO_INSTALL = 1
      };
      /**  */
      Fate fate() const;

      /** True if neither to delete or to install */
      bool unmodified() const
      { return fate() == UNMODIFIED; }

      /** True if locked (subclass of unmodified).
       * The \ref locked status indicates that \b all installed and/or \b all available
       * items are locked. So you can't remove and/or can't install any item. This
       * is the common case.
       * \see \ref hasLocks
       */
      bool locked() const
      { Status st( status() ); return( st == S_Protected || st == S_Taboo ); }

      /** True if it includes locked items (don't mix this with the \ref locked status).
       * This is also \c true for partially locked items, where only specific versions
       * are locked.
       * \see \ref locked
       */
      bool hasLocks() const;

      /** True if either to delete or to install */
      bool toModify() const
      { return fate() != UNMODIFIED; }

      /** True if to delete */
      bool toDelete() const
      { return fate() == TO_DELETE; }

      /** True if to install */
      bool toInstall() const
      { return fate() == TO_INSTALL; }

      /** True if would be on system after commit. */
      bool onSystem() const
      { return( ( hasInstalledObj() && !toDelete() )
              ||( hasCandidateObj() && toInstall() ) ); }

      /** True if would be off system after commit. */
      bool offSystem() const
      { return ! onSystem(); }

      /** */
      bool setFate( Fate fate_r, ResStatus::TransactByValue causer_r = ResStatus::USER );

      /** Set the item to be installed (new- or re-install). */
      bool setToInstall( ResStatus::TransactByValue causer_r = ResStatus::USER )
      { return setFate( TO_INSTALL, causer_r ); }

      /** Take care the item gets installed if it is not. */
      bool setInstalled( ResStatus::TransactByValue causer_r = ResStatus::USER );

      /** Take care the item gets installed if it is not, or is older. */
      bool setUpToDate( ResStatus::TransactByValue causer_r = ResStatus::USER );

      /** Set the item to be deleted (must be installed). */
      bool setToDelete( ResStatus::TransactByValue causer_r = ResStatus::USER )
      { return setFate( TO_DELETE, causer_r ); }

      /** Take care the item gets deleted if it is installed. */
      bool setDeleted( ResStatus::TransactByValue causer_r = ResStatus::USER );

      /** Set the item to stay unmodified. */
      bool unset( ResStatus::TransactByValue causer_r = ResStatus::USER )
      { return setFate( UNMODIFIED, causer_r ); }
      //@}

    public:
      /**
       * \name Special inteface for Y2UI.
       * \note This interface acts on \ref ResStatus::USER level.
       * The \ref Status enum, and allowed state transitions are
       * tightly related to the Y2UI.
      */
      //@{
      /** Return the current Status */
      Status status() const;

      /**
       * Try to set a new Status.
       * Returns \c false if the transitions is not allowed.
       */
      bool setStatus( Status state_r, ResStatus::TransactByValue causer_r = ResStatus::USER );

      /** Return who caused the modification. */
      ResStatus::TransactByValue modifiedBy() const;

      /** Return value of LicenceConfirmed bit. */
      bool hasLicenceConfirmed() const;

      /** Set LicenceConfirmed bit. */
      void setLicenceConfirmed( bool val_r = true );
      //@}

    public:
      /** Implementation  */
      struct Impl;
      using Impl_Ptr = shared_ptr<Impl>;
      /** Default ctor */
      Selectable( Impl_Ptr pimpl_r );
    private:
      /** Dtor */
      ~Selectable() override;
    private:
      /** Pointer to implementation */
      RW_pointer<Impl> _pimpl;
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates Selectable Stream output */
    std::ostream & operator<<( std::ostream & str, const Selectable & obj ) ZYPP_API;

    /** \relates Selectable More verbose stream output */
    std::ostream & dumpOn( std::ostream & str, const Selectable & obj ) ZYPP_API;

    /** Solvable to Selectable transform functor.
     * \relates Selectable
     * \relates sat::SolvIterMixin
     */
    struct ZYPP_API asSelectable
    {
      using result_type = Selectable_Ptr;

      Selectable_Ptr operator()( const sat::Solvable & solv_r ) const;

      Selectable_Ptr operator()( const PoolItem & pi_r ) const
      { return operator()( pi_r.satSolvable() ); }
    };

    /////////////////////////////////////////////////////////////////
  } // namespace ui
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_UI_SELECTABLE_H
