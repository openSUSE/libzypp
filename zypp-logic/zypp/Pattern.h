/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/Pattern.h
 *
*/
#ifndef ZYPP_PATTERN_H
#define ZYPP_PATTERN_H

#include <zypp/ResObject.h>
#include <zypp/Pathname.h>
#include <zypp/sat/SolvableSet.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  DEFINE_PTR_TYPE(Pattern);

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Pattern
  //
  /** Pattern interface.
  */
  class ZYPP_API Pattern : public ResObject
  {
    public:
      using Self = Pattern;
      using TraitsType = ResTraits<Self>;
      using Ptr = TraitsType::PtrType;
      using constPtr = TraitsType::constPtrType;

    public:
      using NameList = sat::ArrayAttr<IdString, IdString>;
      using Contents = sat::SolvableSet;

    public:
      /** */
      bool isDefault() const;
      /** */
      bool userVisible() const;
      /** */
      std::string category( const Locale & lang_r = Locale() ) const;
      /** */
      Pathname icon() const;
      /** */
      Pathname script() const;
      /** */
      std::string order() const;

    public:
      /** \name Auto patterns (libyzpp-14)
       * Patterns are no longer defined by separate metadate files, but via
       * special dependencies provided by a corresponding patterns- package.
       * The pattern itself requires only its patterns- package. The package
       * contains all further dependencies.
       * This way, patterns are no longer pseudo-installed objects with a computed
       * status, but installed, iff the corresponding patterns- package is
       * installed.
       */
      //@{
      /** This patterns is auto-defined by a patterns- package. */
      bool isAutoPattern() const;
      /** The corresponding patterns- package if \ref isAutoPattern. */
      sat::Solvable autoPackage() const;
      //@}
    public:
      /** Ui hint: included patterns. */
      NameList includes() const;

      /** Ui hint: patterns this one extends. */
      NameList extends() const;

      /** Ui hint: Required Packages. */
      Contents core() const;

      /** Ui hint: Dependent packages.
       * This also includes recommended and suggested (optionally exclude) packages.
      */
      Contents depends( bool includeSuggests_r = true ) const;
      /** \overload Without SUGGESTS. */
      Contents dependsNoSuggests() const
      { return depends( false ); }

      /** The collection of packages associated with this pattern.
       * This also evaluates the patterns includes/extends relation.
       * Optionally exclude \c SUGGESTED packages.
       */
      Contents contents( bool includeSuggests_r = true ) const;
      /** \overload Without SUGGESTS. */
      Contents contentsNoSuggests() const
      { return contents( false ); }

    public:
      struct ContentsSet
      {
        Contents req;	///< required content set
        Contents rec;	///< recommended content set
        Contents sug;	///< suggested content set
      };
      /** Dependency based content set (does not evaluate includes/extends relation).
       * If \a recursively_r, required and recommended
       * patterns are recursively expanded.
       */
      void contentsSet( ContentsSet & collect_r, bool recursively_r = false ) const;
      /** \overload Convenience for recursively expanded contentsSet */
      void fullContentsSet( ContentsSet & collect_r ) const
      { return contentsSet( collect_r, /*recursively_r*/true ); }

    protected:
      friend Ptr make<Self>( const sat::Solvable & solvable_r );
      /** Ctor */
      Pattern( const sat::Solvable & solvable_r );
      /** Dtor */
      ~Pattern() override;
  };
  ///////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_PATTERN_H
