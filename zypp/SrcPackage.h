/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/SrcPackage.h
 *
*/
#ifndef ZYPP_SRCPACKAGE_H
#define ZYPP_SRCPACKAGE_H

#include <zypp/ResObject.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  DEFINE_PTR_TYPE(SrcPackage);

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : SrcPackage
  //
  /** SrcPackage interface.
  */
  class ZYPP_API SrcPackage : public ResObject
  {

  public:
    using Self = SrcPackage;
    using TraitsType = ResTraits<Self>;
    using Ptr = TraitsType::PtrType;
    using constPtr = TraitsType::constPtrType;

  public:
    /** The type of the source rpm ("src" or "nosrc"). */
    std::string sourcePkgType() const;

    /** location of resolvable in repo */
    OnMediaLocation location() const;

    /** Location of the downloaded package in cache or an empty path. */
    Pathname cachedLocation() const;

    /** Whether the package is cached. */
    bool isCached() const
    { return ! cachedLocation().empty(); }

  protected:
    friend Ptr make<Self>( const sat::Solvable & solvable_r );
    /** Ctor */
    SrcPackage( const sat::Solvable & solvable_r );
    /** Dtor */
    ~SrcPackage() override;
  };
  ///////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SRCPACKAGE_H
