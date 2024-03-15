/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/Package.h
 *
*/
#ifndef ZYPP_PACKAGE_H
#define ZYPP_PACKAGE_H

#include <zypp/ResObject.h>
#include <zypp/PackageKeyword.h>
#include <zypp/Changelog.h>
#include <zypp/VendorSupportOptions.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  DEFINE_PTR_TYPE(Package);

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Package
  //
  /** Package interface.
  */
  class Package : public ResObject
  {
  public:
    using Self = Package;
    using TraitsType = ResTraits<Self>;
    using Ptr = TraitsType::PtrType;
    using constPtr = TraitsType::constPtrType;

  public:
    using Keywords = sat::ArrayAttr<PackageKeyword, IdString>;
    using FileList = sat::ArrayAttr<std::string, std::string>;

  public:

    /**
     * Returns the level of supportability the vendor
     * gives to this package.
     *
     * If the identical package happens to appear in multiple
     * repos with different support levels, the maximum
     * level is returned.
     *
     * This is one value from \ref VendorSupportOption.
     */
    VendorSupportOption vendorSupport() const;

    /**
     * True if the vendor support for this package
     * is unknown or explicitly unsupported.
     */
    bool maybeUnsupported() const;

    /** The name(s) of the successor package if \ref vendorSupport is \ref VendorSupportSuperseded.
     * Ideally only one name, but it might be that different repos
     * provide different successor names. These are the pure metadata
     * values. \see \ref supersededByItems and \ref ui::Selectable::supersededBy
     */
    std::vector<std::string> supersededBy() const;

    /** The successor package(s) if \ref vendorSupport is \ref VendorSupportSuperseded.
     * Each name returned by \ref supersededBy is resolved into the
     * \ref Solvable::ident of an Item in the pool (collapsing chains
     * of superseeded packages).
     *
     * The std::pair returned contains the IdString idents of superseeding packages and
     * any std::strings which could not be resolved into a package name.
     *
     * Ideally you get back one IdString and no unresolved names. Multiple IdStrings
     * express a choice. Unresolved names hint to broken repo metadata, as superseeding
     * packages should be available in the repo.
     */
    std::pair<std::vector<IdString>,std::vector<std::string>> supersededByItems() const;

    /** Get the package change log */
    Changelog changelog() const;
    /** */
    std::string buildhost() const;
    /** */
    std::string distribution() const;
    /** */
    std::string license() const;
    /** */
    std::string packager() const;
    /** */
    std::string group() const;
    /** */
    Keywords keywords() const;
    /** Don't ship it as class Url, because it might be
     * in fact anything but a legal Url. */
    std::string url() const;
    /** Size of corresponding the source package. */
    ByteCount sourcesize() const;
    /** */
    std::list<std::string> authors() const;

    /** Return the packages filelist (if available).
     * The returned \ref FileList appears to be a container of
     * \c std::string. In fact it is a query, so it does not
     * consume much memory.
    */
    FileList filelist() const;

    /** \name Source package handling
    */
    //@{
    /** Name of the source rpm this package was built from.
     */
    std::string sourcePkgName() const;

    /** Edition of the source rpm this package was built from.
     */
    Edition sourcePkgEdition() const;

    /** The type of the source rpm (\c "src" or \c "nosrc").
     */
    std::string sourcePkgType() const;

    /** The source rpms \c "name-version-release.type"
     */
    std::string sourcePkgLongName() const;
    //@}

    /**
     * Checksum the source says this package should have.
     * \see \ref location
     */
    CheckSum checksum() const;

    /** Location of the resolvable in the repository.
     * \ref OnMediaLocation conatins all information required to
     * retrieve the packge (url, checksum, etc.).
     */
    OnMediaLocation location() const;

    /** Location of the downloaded package in cache or an empty path. */
    Pathname cachedLocation() const;

    /** Whether the package is cached. */
    bool isCached() const
    { return ! cachedLocation().empty(); }

  protected:
    friend Ptr make<Self>( const sat::Solvable & solvable_r );
    /** Ctor */
    Package( const sat::Solvable & solvable_r );
    /** Dtor */
    ~Package() override;
  };

  ///////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_PACKAGE_H
