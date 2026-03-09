/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/Solvable.h
 *
*/
#ifndef ZYPP_SAT_SOLVABLE_H
#define ZYPP_SAT_SOLVABLE_H

#include <iosfwd>

#include <zypp-core/ng/meta/type_traits.h>
#include <zypp-core/ByteCount.h>
#include <zypp-core/Date.h>
#include <zypp-core/CheckSum.h>

#include <zypp/ng/sat/poolconstants.h>
#include <zypp/ng/sat/poolmember.h>
#include <zypp/ng/sat/solvattr.h>
#include <zypp/ng/restraits.h>
#include <zypp/ng/idstring.h>
#include <zypp/ng/edition.h>
#include <zypp/ng/arch.h>
#include <zypp/ng/cpeid.h>
#include <zypp/ng/dep.h>
#include <zypp/ng/sat/capabilities.h>
#include <zypp/ng/sat/capability.h>
#include <zypp/ng/reskind.h>
#include <zypp/ng/locale.h>



namespace zyppng
{
  ///////////////////////////////////////////////////////////////////
  namespace sat
  {
    ///////////////////////////////////////////////////////////////////
    /// \class Solvable
    /// \brief  A \ref Solvable object within the sat \ref Pool.
    ///
    /// \note Unfortunately libsolv combines the objects kind and
    /// name in a single identifier \c "pattern:kde_multimedia",
    /// \b except for packages and source packes. They are not prefixed
    /// by any kind string. Instead the architecture is abused to store
    /// \c "src" and \c "nosrc" values.
    ///
    /// \ref Solvable will hide this inconsistency by treating source
    /// packages as an own kind of solvable and map their arch to
    /// \ref Arch_noarch.
    ///////////////////////////////////////////////////////////////////
    class Solvable : public PoolMember
    {
    public:
      using IdType = detail::SolvableIdType;

      static const IdString patternToken;	///< Indicator provides `pattern()`
      static const IdString productToken;	///< Indicator provides `product()`

      static const IdString retractedToken;	///< Indicator provides `retracted-patch-package()`
      static const IdString ptfMasterToken;	///< Indicator provides `ptf()`
      static const IdString ptfPackageToken;	///< Indicator provides `ptf-package()`

    public:
      /** Default ctor creates \ref noSolvable.*/
      Solvable()
        : _id( detail::noSolvableId )
      {}

      Solvable(const Solvable &) = default;
      Solvable(Solvable &&) noexcept = default;
      Solvable &operator=(const Solvable &) = default;
      Solvable &operator=(Solvable &&) noexcept = default;

      /** \ref PoolImpl ctor. */
      explicit Solvable( IdType id_r )
      : _id( id_r )
      {}

    public:
      /** Represents no \ref Solvable. */
      static const Solvable noSolvable;

      /** Evaluate \ref Solvable in a boolean context (\c != \c noSolvable). */
      explicit operator bool() const
      { return get(); }

    public:
      /** The identifier.
       * This is the solvables \ref name, \b except for packages and
       * source packes, prefixed by its \ref kind.
       */
      IdString ident()const;

      /** The Solvables ResKind. */
      ResKind kind()const;

      /** Test whether a Solvable is of a certain \ref ResKind.
       * The test is far cheaper than actually retrieving and
       * comparing the \ref kind.
       */
      bool isKind( const ResKind & kind_r ) const;
      /** \overload */
      template<class TRes>
      bool isKind() const
      { return isKind( resKind<TRes>() ); }
      /** \overload Extend the test to a range of \ref ResKind. */
      template<class TIterator>
      bool isKind( TIterator begin, TIterator end ) const
      { for_( it, begin, end ) if ( isKind( *it ) ) return true; return false; }

      /** The name (without any ResKind prefix). */
      std::string name() const;

      /** The edition (version-release). */
      Edition edition() const;

      /** The architecture. */
      Arch arch() const;

      /** The vendor. */
      IdString vendor() const;

      /** The \ref repo id this \ref Solvable belongs to. */
      detail::RepoIdType repository() const;

      /** Return whether this \ref Solvable belongs to the system repo.
       * \note This includes the otherwise hidden systemSolvable.
       */
      bool isSystem() const;

      /** The items build time. */
      zypp::Date buildtime() const;

      /** The items install time (\c false if not installed). */
      zypp::Date installtime() const;

      /** Test whether two Solvables have the same content.
       * Basically the same name, edition, arch, vendor and buildtime.
       */
      bool identical( const Solvable & rhs ) const;

      /** Test for same name-version-release.arch */
      bool sameNVRA( const Solvable & rhs ) const
      { return( get() == rhs.get() || ( ident() == rhs.ident() && edition() == rhs.edition() && arch() == rhs.arch() ) ); }

    public:
      /** \name Access to the \ref Solvable dependencies.
       *
       * \note Prerequires are a subset of requires.
       */
      //@{
      Capabilities dep_provides()    const;
      Capabilities dep_requires()    const;
      Capabilities dep_conflicts()   const;
      Capabilities dep_obsoletes()   const;
      Capabilities dep_recommends()  const;
      Capabilities dep_suggests()    const;
      Capabilities dep_enhances()    const;
      Capabilities dep_supplements() const;
      Capabilities dep_prerequires() const;

      /** Return \ref Capabilities selected by \ref Dep constant. */
      Capabilities dep( Dep which_r ) const
      {
        switch( which_r.inSwitch() )
        {
          case Dep::PROVIDES_e:    return dep_provides();    break;
          case Dep::REQUIRES_e:    return dep_requires();    break;
          case Dep::CONFLICTS_e:   return dep_conflicts();   break;
          case Dep::OBSOLETES_e:   return dep_obsoletes();   break;
          case Dep::RECOMMENDS_e:  return dep_recommends();  break;
          case Dep::SUGGESTS_e:    return dep_suggests();    break;
          case Dep::ENHANCES_e:    return dep_enhances();    break;
          case Dep::SUPPLEMENTS_e: return dep_supplements(); break;
          case Dep::PREREQUIRES_e: return dep_prerequires(); break;
        }
        return Capabilities();
      }
      /** \overload operator[] */
      Capabilities operator[]( Dep which_r ) const
      { return dep( which_r ); }


      /** Return the namespaced provides <tt>'namespace([value])[ op edition]'</tt> of this Solvable. */
      CapabilitySet providesNamespace( const std::string & namespace_r ) const;

      /** Return <tt>'value[ op edition]'</tt> for namespaced provides <tt>'namespace(value)[ op edition]'</tt>.
       * Similar to \ref providesNamespace, but the namespace is stripped from the
       * dependencies. This is convenient if the namespace denotes packages that
       * should be looked up. E.g. the \c weakremover namespace used in a products
       * release package denotes the packages that were dropped from the distribution.
       * \see \ref Product::droplist
       */
      CapabilitySet valuesOfNamespace( const std::string & namespace_r ) const;
      //@}

      std::pair<bool, CapabilitySet> matchesSolvable ( const SolvAttr &attr, const Solvable &solv ) const;

    public:
      /** \name Locale support. */
      //@{
      /** Whether this \c Solvable claims to support locales. */
      bool supportsLocales() const;
      /** Whether this \c Solvable supports a specific \ref Locale. */
      bool supportsLocale( const Locale & locale_r ) const;
      /** Whether this \c Solvable supports at least one of the specified locales. */
      bool supportsLocale( const LocaleSet & locales_r ) const;
      /** Return the supported locales. */
      LocaleSet getSupportedLocales() const;
      /** \overload Legacy return via arg \
    ///////////////////////////////////////////////////////////////////a locales_r */
      void getSupportedLocales( LocaleSet & locales_r ) const
      { locales_r = getSupportedLocales(); }
      //@}

    public:
      /** The solvables CpeId if available. */
      CpeId cpeId() const;

      /** Media number the solvable is located on (\c 0 if no media access required). */
      unsigned mediaNr() const;

      /** Installed (unpacked) size.
       * This is just a total number. Many objects provide even more detailed
       * disk usage data. You can use \ref DiskUsageCounter to find out
       * how objects data are distributed across partitions/directories.
       * \code
       *   // Load directory set into ducounter
       *   DiskUsageCounter ducounter( { "/", "/usr", "/var" } );
       *
       *   // see how noch space the packages use
       *   for ( const PoolItem & pi : pool )
       *   {
       *     cout << pi << ducounter.disk_usage( pi ) << endl;
       *     // I__s_(7)GeoIP-1.4.8-3.1.2.x86_64(@System) {
       *     // dir:[/] [ bs: 0 B ts: 0 B us: 0 B (+-: 1.0 KiB)]
       *     // dir:[/usr] [ bs: 0 B ts: 0 B us: 0 B (+-: 133.0 KiB)]
       *     // dir:[/var] [ bs: 0 B ts: 0 B us: 0 B (+-: 1.1 MiB)]
       *     // }
       *   }
       * \endcode
       * \see \ref DiskUsageCounter
       */
      zypp::ByteCount installSize() const;

      /** Download size. */
      zypp::ByteCount downloadSize() const;

      /** The distribution string. */
      std::string distribution() const;

      /** Short (singleline) text describing the solvable (opt. translated). */
      std::string summary( const Locale & lang_r = Locale() ) const;

      /** Long (multiline) text describing the solvable (opt. translated). */
      std::string description( const Locale & lang_r = Locale() ) const;

      /** UI hint text when selecting the solvable for install (opt. translated). */
      std::string insnotify( const Locale & lang_r = Locale() ) const;
      /** UI hint text when selecting the solvable for uninstall (opt. translated).*/
      std::string delnotify( const Locale & lang_r = Locale() ) const;

      /** License or agreement to accept before installing the solvable (opt. translated). */
      std::string licenseToConfirm( const Locale & lang_r = Locale() ) const;
      /** \c True except for well known exceptions (i.e show license but no need to accept it). */
      bool needToAcceptLicense() const;

    public:
      /** \name Attribute lookup.
       * \see \ref LookupAttr and  \ref ArrayAttr providing a general, more
       * query like interface for attribute retrieval.
       */
      //@{
      /**
       * returns the string attribute value for \ref attr
       * or an empty string if it does not exists.
       */
      std::string lookupStrAttribute( const SolvAttr & attr ) const;
      /** \overload Trying to look up a translated string attribute.
       *
       * Returns the translation for \c lang_r.
       *
       * Passing an empty \ref Locale will return the string for the
       * current default locale (\see \ref ZConfig::TextLocale),
       * \b considering all fallback locales.
       *
       * Returns an empty string if no translation is available.
       */
      std::string lookupStrAttribute( const SolvAttr & attr, const Locale & lang_r ) const;

      /**
       * returns the numeric attribute value for \ref attr
       * or 0 if it does not exists.
       */
      unsigned long long lookupNumAttribute( const SolvAttr & attr ) const;
      /** \overload returning custom notfound_r value */
      unsigned long long lookupNumAttribute( const SolvAttr & attr, unsigned long long notfound_r ) const;

      /**
       * returns the boolean attribute value for \ref attr
       * or \c false if it does not exists.
       */
      bool lookupBoolAttribute( const SolvAttr & attr ) const;

      /**
       * returns the id attribute value for \ref attr
       * or \ref detail::noId if it does not exists.
       */
      detail::IdType lookupIdAttribute( const SolvAttr & attr ) const;

      /**
       * returns the CheckSum attribute value for \ref attr
       * or an empty CheckSum if ir does not exist.
       */
      zypp::CheckSum lookupCheckSumAttribute( const SolvAttr & attr ) const;

    public:
      /** Return next Solvable in \ref Pool (or \ref noSolvable). */
      Solvable nextInPool() const;
      /** Return next Solvable in \ref Repo (or \ref noSolvable). */
      Solvable nextInRepo() const;
      /** Expert backdoor. */
      detail::CSolvable * get() const;
      /** Expert backdoor. */
      IdType id() const { return _id; }

    private:
      IdType _id;
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates Solvable */
    inline bool operator==( const Solvable & lhs, const Solvable & rhs )
    { return lhs.get() == rhs.get(); }

    /** \relates Solvable */
    inline bool operator!=( const Solvable & lhs, const Solvable & rhs )
    { return lhs.get() != rhs.get(); }

    /** \relates Solvable */
    inline bool operator<( const Solvable & lhs, const Solvable & rhs )
    { return lhs.get() < rhs.get(); }

    /** \relates Solvable Test whether a \ref Solvable is of a certain Kind. */
    template<class TRes>
    inline bool isKind( const Solvable & solvable_r )
    { return solvable_r.isKind( ResTraits<TRes>::kind ); }

    /** \relates Solvable Test for same content. */
    inline bool identical( const Solvable & lhs, const Solvable & rhs )
    { return lhs.identical( rhs ); }

    /** \relates Solvable Test for same name version release and arch. */
    inline bool sameNVRA( const Solvable & lhs, const Solvable & rhs )
    { return lhs.sameNVRA( rhs ); }


    /** \relates Solvable Compare according to \a kind and \a name. */
    inline int compareByN( const Solvable & lhs, const Solvable & rhs )
    {
      int res = 0;
      if ( lhs != rhs )
      {
        if ( (res = lhs.kind().compare( rhs.kind() )) == 0 )
          res = lhs.name().compare( rhs.name() );
      }
      return res;
    }

    /** \relates Solvable Compare according to \a kind, \a name and \a edition. */
    inline int compareByNVR( const Solvable & lhs, const Solvable & rhs )
    {
      int res = compareByN( lhs, rhs );
      if ( res == 0 )
        res = lhs.edition().compare( rhs.edition() );
      return res;
    }

    /** \relates Solvable Compare according to \a kind, \a name, \a edition and \a arch. */
    inline int compareByNVRA( const Solvable & lhs, const Solvable & rhs )
    {
      int res = compareByNVR( lhs, rhs );
      if ( res == 0 )
        res = lhs.arch().compare( rhs.arch() );
      return res;
    }

    ///////////////////////////////////////////////////////////////////
    namespace detail
    {
      ///////////////////////////////////////////////////////////////////
      /// \class SolvableIterator
      /// \brief Iterate over valid Solvables in the pool.
      /// If the Solvable passed to the ctor is not valid, advance
      /// to the next valid solvable (or Solvable::noSolvable if the
      /// end is reached,
      class SolvableIterator : public boost::iterator_adaptor<
          SolvableIterator                   // Derived
          , CSolvable*                       // Base
          , const Solvable                   // Value
          , boost::forward_traversal_tag     // CategoryOrTraversal
          , const Solvable                   // Reference
          >
      {
        public:
          SolvableIterator()
          : SolvableIterator::iterator_adaptor_( nullptr )
          {}

          explicit SolvableIterator( const Solvable & val_r )
          : SolvableIterator::iterator_adaptor_( nullptr )
          { initialAssignVal( val_r ); }

          explicit SolvableIterator( SolvableIdType id_r )
          : SolvableIterator::iterator_adaptor_( nullptr )
          { initialAssignVal( Solvable(id_r) ); }

        private:
          friend class boost::iterator_core_access;

          Solvable dereference() const
          { return _val; }

          void increment()
          { assignVal( _val.nextInPool() ); }

        private:
          void initialAssignVal( const Solvable & val_r )
          { assignVal( val_r ? val_r : val_r.nextInPool() ); }

          void assignVal( const Solvable & val_r )
          { _val = val_r; base_reference() = _val.get(); }

          Solvable _val;
      };
    } // namespace detail

    namespace detail {

      template <typename T>
      using satSolvable_t = decltype(std::declval<T>().satSolvable());

      template <typename T>
      constexpr bool has_satSolvable_v = std::is_detected_v<satSolvable_t, T>;

      template <typename T, typename = void >
      struct convert_to_solvable{
          // static_assert to force specialization/implementation
          static_assert( always_false_v<T>, "Type must have .satSolvable() or a specialization of detail::convert_to_solvable" );
      };

      template <typename T>
      struct convert_to_solvable<T, std::void_t<satSolvable_t<T>>> {
          template <typename SType = T>
          static Solvable asSolvable( SType && obj ) {
            return std::forward<SType>(obj).satSolvable();
          }
      };
    }

    /** To Solvable transform functor.
     * \relates Solvable
     * \relates sat::SolvIterMixin
     */
    struct asSolvable
    {
      using result_type = Solvable;

      Solvable operator()( const Solvable & solv_r ) const
      { return solv_r; }

      template <typename T>
      Solvable operator()( T&& obj )const {
        return detail::convert_to_solvable<T>::asSolvable( std::forward<T>(obj));
      }
    };

  } // namespace sat
} // namespace zypp

ZYPP_DEFINE_ID_HASHABLE( ::zyppng::sat::Solvable );

#endif // ZYPP_SAT_SOLVABLE_H
