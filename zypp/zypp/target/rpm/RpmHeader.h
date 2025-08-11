/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/target/rpm/RpmHeader.h
 *
*/
#ifndef ZYPP_TARGET_RPM_RPMHEADER_H
#define ZYPP_TARGET_RPM_RPMHEADER_H

#include <iosfwd>
#include <list>
#include <utility>

#include <zypp/target/rpm/BinHeader.h>

#include <zypp/Package.h>
#include <zypp/Changelog.h>
#include <zypp/Pathname.h>


namespace zypp
{
namespace target
{
namespace rpm
{

struct FileInfo
{
  Pathname    filename;
  ByteCount   size;
  std::string md5sum;
  uid_t       uid;
  gid_t       gid;
  mode_t      mode;
  time_t      mtime;
  bool        ghost;
  Pathname link_target;
};

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : RpmHeader
/**
 * @short Wrapper class for rpm header struct.
 *
 * <code>RpmHeader</code> provides methods to query the content
 * of a rpm header struct retrieved from the RPM database or by reading
 * the rpm header of a package on disk.
 *
 * The rpm header contains all data associated with a package. So you
 * probabely do not want to permanently store too many of them.
 *
 * <B>NEVER create <code>RpmHeader</code> from a NULL <code>Header</code>! </B>
 **/
class ZYPP_API RpmHeader : public BinHeader
{
public:
  using Ptr = intrusive_ptr<RpmHeader>;
  using constPtr = intrusive_ptr<const RpmHeader>;

private:

  CapabilitySet PkgRelList_val( tag tag_r, bool pre, std::set<std::string> * freq_r = 0 ) const;

public:

  /**
   *
   **/
  RpmHeader( Header h_r = 0 );

  /**
   * <B>Dangerous!<\B> This one takes the header out of rhs
   * and leaves rhs empty.
   **/
  RpmHeader( BinHeader::Ptr & rhs );

  ~RpmHeader() override;

  bool isSrc() const;	///< Either 'src' or 'nosrc'
  bool isNosrc() const;	///< Only 'nosrc'

  std::string ident() const; ///< N-V-R.A or empty

public:

  std::string      tag_name()    const;
  Edition::epoch_t tag_epoch()   const;
  std::string      tag_version() const;
  std::string      tag_release() const;
  Edition          tag_edition() const;
  Arch             tag_arch()    const;

  Date tag_installtime() const;
  Date tag_buildtime()   const;

  /**
   * If <code>freq_r</code> is not NULL, file dependencies found are inserted.
   **/
  CapabilitySet tag_provides ( std::set<std::string> * freq_r = 0 ) const;
  /**
   * @see #tag_provides
   **/
  CapabilitySet tag_requires ( std::set<std::string> * freq_r = 0 ) const;
  /**
   * @see #tag_provides
   **/
  CapabilitySet tag_prerequires ( std::set<std::string> * freq_r = 0 ) const;
  /**
   * @see #tag_provides
   **/
  CapabilitySet tag_conflicts( std::set<std::string> * freq_r = 0 ) const;
  /**
   * @see #tag_provides
   **/
  CapabilitySet tag_obsoletes( std::set<std::string> * freq_r = 0 ) const;
  /**
   * @see #tag_provides
   **/
  CapabilitySet tag_enhances( std::set<std::string> * freq_r = 0 ) const;
  /**
   * @see #tag_provides
   **/
  CapabilitySet tag_suggests( std::set<std::string> * freq_r = 0 ) const;
  /**
   * @see #tag_provides
   **/
  CapabilitySet tag_supplements( std::set<std::string> * freq_r = 0 ) const;
  /**
   * @see #tag_provides
   **/
  CapabilitySet tag_recommends( std::set<std::string> * freq_r = 0 ) const;

  ByteCount tag_size()        const;
  ByteCount tag_archivesize() const;

  std::string tag_summary()      const;
  std::string tag_description()  const;
  std::string tag_group()        const;
  std::string tag_vendor()       const;
  std::string tag_distribution() const;
  std::string tag_license()      const;
  std::string tag_buildhost()    const;
  std::string tag_packager()     const;
  std::string tag_url()          const;
  std::string tag_os()           const;
  std::string tag_prein()        const;
  std::string tag_preinprog()    const;
  std::string tag_postin()       const;
  std::string tag_postinprog()   const;
  std::string tag_preun()        const;
  std::string tag_preunprog()    const;
  std::string tag_postun()       const;
  std::string tag_postunprog()   const;
  std::string tag_pretrans()     const;
  std::string tag_pretransprog() const;
  std::string tag_posttrans()    const;
  std::string tag_posttransprog()const;
  std::string tag_sourcerpm()    const;

  /**
   * Uses headerFormat to query the signature info from the header.
   * \note Query copied from Yum misc/checksig.py
   */
  std::string signatureKeyID()   const;

  /** just the list of names  */
  std::list<std::string> tag_filenames() const;

  /**
   * complete information about the files
   * (extended version of tag_filenames())
          */
  std::list<FileInfo> tag_fileinfos() const;

  Changelog tag_changelog() const;

public:

  std::ostream & dumpOn( std::ostream & str ) const override;

public:

  /**
   * Digest and signature verification flags
   **/
  enum VERIFICATION
  {
    VERIFY       = 0x0000,
    NODIGEST     = (1<<0),
    NOSIGNATURE  = (1<<1),
    NOVERIFY     = 0xffff
  };

  /**
   * Get an accessible packages data from disk.
   * Returns NULL on any error.
   **/
  static RpmHeader::constPtr readPackage( const Pathname & path,
                                          VERIFICATION verification = VERIFY );

  /**
   * Get an accessible packages data from disk using a existing transaction.
   * Returns a std::pair container the header and resultcode from reading it
   **/
  static std::pair<RpmHeader::Ptr, int> readPackage( rpmts ts_r, const Pathname & path_r );
};

///////////////////////////////////////////////////////////////////
} // namespace rpm
} // namespace target
} // namespace zypp

#endif // ZYPP_TARGET_RPM_RPMHEADER_H

