/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/Package.cc
 *
*/
#include <iostream>
#include <fstream>

#include <zypp/base/LogTools.h>
#include <zypp/base/String.h>
#include <zypp/base/StringV.h>
#include <zypp/Package.h>
#include <zypp/sat/LookupAttr.h>
#include <zypp/ZYppFactory.h>
#include <zypp/target/rpm/RpmDb.h>
#include <zypp/target/rpm/RpmHeader.h>
#include <zypp/ui/Selectable.h>

#include <zypp/ng/repoinfo.h>

///////////////////////////////////////////////////////////////////
namespace zyppintern
{
  using namespace zypp;

  /** getVendorSupportOption return value */
  using GetVendorSupportOptionReturn = std::pair<VendorSupportOption,std::optional<std::set<std::string_view>>>;

  /** Get VendorSupportOption and the PKGNAME if VendorSupportSuperseded */
  GetVendorSupportOptionReturn getVendorSupportOption( sat::Solvable solv_r )
  {
    static const IdString support_unsupported( "support_unsupported" );
    static const IdString support_acc( "support_acc" );
    static const IdString support_l1( "support_l1" );
    static const IdString support_l2( "support_l2" );
    static const IdString support_l3( "support_l3" );
    static const IdString support_superseded( "support_superseded" ); // opt. followed by "(PKGNAME)"

    GetVendorSupportOptionReturn ret { VendorSupportUnknown, std::nullopt };

    // max over all identical packages
    for ( const auto & solv : sat::WhatProvides( (Capability(solv_r.ident().id())) ) )
    {
      if ( solv.edition() == solv_r.edition()
        && solv.ident() == solv_r.ident()
        && solv_r.identical( solv ) )
      {
        for ( PackageKeyword kw : Package::Keywords( sat::SolvAttr::keywords, solv ) )
        {
          switch ( ret.first )
          {
            case VendorSupportUnknown:
              if ( kw == support_unsupported )	{ ret.first = VendorSupportUnsupported; break; }
            case VendorSupportUnsupported:
              if ( kw == support_acc )	{ ret.first = VendorSupportACC; break; }
            case VendorSupportACC:
              if ( kw == support_l1 )	{ ret.first = VendorSupportLevel1; break; }
            case VendorSupportLevel1:
              if ( kw == support_l2 )	{ ret.first = VendorSupportLevel2; break; }
            case VendorSupportLevel2:
              if ( kw == support_l3 )	{ ret.first = VendorSupportLevel3; break; }
            case VendorSupportLevel3:
            case VendorSupportSuperseded:
              // VendorSupportSuperseded is special as the kw may also contain a '(PKGNAME)'.
              // In fact even multiple (PKGNAMES) derived from multiple keywords.
              if ( strv::hasPrefix( kw.asStringView(), support_superseded.asStringView() ) ) {
                ret.first = VendorSupportSuperseded;
                if ( kw.c_str()[support_superseded.size()] == '(' && kw.c_str()[kw.size()-1] == ')' ) {
                  std::string_view val { kw.c_str()+support_superseded.size()+1, kw.size()-support_superseded.size()-2 };
                  if ( not val.empty() ) {
                    if ( not ret.second )
                      ret.second = std::set<std::string_view>();
                    ret.second->insert( val );
                  } else {
                    WAR << "No superseding package defined in " << kw << endl;
                  }
                } else {
                  WAR << "No superseding package defined in " << kw << endl;
                }
              }
          }
        }
      }
    }
    return ret;
  }

  inline bool schemeIsLocalDir( const Url & url_r )
  {
      const std::string & s( url_r.getScheme() );
      return s == "dir" || s == "file";
  }

  // here and from SrcPackage.cc
  Pathname cachedLocation( const OnMediaLocation & loc_r, const zyppng::RepoInfo & repo_r )
  {
    PathInfo pi( repo_r.packagesPath() / repo_r.path() / loc_r.filename() );

    if ( ! pi.isExist() )
      return Pathname();	// no file in cache

    if ( loc_r.checksum().empty() )
    {
      Url url( repo_r.url() );
      if ( ! schemeIsLocalDir( url ) )
        return Pathname();	// same name but no checksum to verify

      // for local repos compare with the checksum in repo
      if ( CheckSum( CheckSum::md5Type(), std::ifstream( (url.getPathName() / repo_r.path() / loc_r.filename()).c_str() ) )
        != CheckSum( CheckSum::md5Type(), std::ifstream( pi.c_str() ) ) )
        return Pathname();	// same name but wrong checksum
    }
    else
    {
      if ( loc_r.checksum() != CheckSum( loc_r.checksum().type(), std::ifstream( pi.c_str() ) ) )
        return Pathname();	// same name but wrong checksum
    }

    return pi.path();		// the right one
  }
} // namespace zyppintern
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  IMPL_PTR_TYPE(Package);

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Package::Package
  //	METHOD TYPE : Ctor
  //
  Package::Package( const sat::Solvable & solvable_r )
  : ResObject( solvable_r )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Package::~Package
  //	METHOD TYPE : Dtor
  //
  Package::~Package()
  {}

  VendorSupportOption Package::vendorSupport() const
  { return zyppintern::getVendorSupportOption( satSolvable() ).first; }

  bool Package::maybeUnsupported() const
  {
    switch ( vendorSupport() )
    {
      case VendorSupportUnknown:
      case VendorSupportUnsupported:
      case VendorSupportACC:
      case VendorSupportSuperseded:
        return true;

      case VendorSupportLevel1:
      case VendorSupportLevel2:
      case VendorSupportLevel3:
        break;	// intentionally no default:
    }
    return false;
  }

  std::vector<std::string> Package::supersededBy() const
  {
    std::vector<std::string> ret;
    const auto & res = zyppintern::getVendorSupportOption( satSolvable() );
    if ( res.second ) {
      // translate the string_view set into stings
      for ( const auto & v : *res.second )
        ret.push_back( std::string( v ) );
    }
    return ret;
  }


  std::pair<std::vector<IdString>,std::vector<std::string>> Package::supersededByItems() const
  {
    std::pair<std::vector<IdString>,std::vector<std::string>> ret;
    std::vector<std::string> todo { supersededBy() };
    if ( not todo.empty() )
    {
      std::set<std::string> done;       // taken from todo and processed
      std::set<std::string> unresolved; // supersededBy does not name a package
      std::set<IdString>    resolved;   // supersededBy resolved to package (not superseded)
      while ( not todo.empty() ) {
        auto doneit { done.insert( std::move(todo.back()) ) };
        todo.pop_back();
        if ( not doneit.second )
          continue; // already processed
        const std::string & next { *doneit.first };

        if ( auto sel = ui::Selectable::get( next ) ) {
          const std::vector<std::string> & more { sel->supersededBy() };
          if ( more.empty() ) {
            resolved.insert( sel->ident() );
          } else {
            todo.insert( todo.end(), more.begin(), more.end() );
          }
        } else {
          unresolved.insert( next );
        }
      }
      if ( not resolved.empty() )
        ret.first.insert( ret.first.end(), resolved.begin(), resolved.end() );
      if ( not unresolved.empty() )
        ret.second.insert( ret.second.end(), unresolved.begin(), unresolved.end() );
    }
    return ret;
  }

  Changelog Package::changelog() const
  {
      Target_Ptr target( getZYpp()->getTarget() );
      if ( ! target )
      {
        ERR << "Target not initialized. Changelog is not available." << std::endl;
        return Changelog();
      }

      if ( repository().isSystemRepo() )
      {
          target::rpm::RpmHeader::constPtr header;
          target->rpmDb().getData(name(), header);
          return header ? header->tag_changelog() : Changelog(); // might be deleted behind our back (bnc #530595)
      }
      WAR << "changelog is not available for uninstalled packages" << std::endl;
      return Changelog();
  }

  std::string Package::buildhost() const
  { return lookupStrAttribute( sat::SolvAttr::buildhost ); }

  std::string Package::distribution() const
  { return lookupStrAttribute( sat::SolvAttr::distribution ); }

  std::string Package::license() const
  { return lookupStrAttribute( sat::SolvAttr::license ); }

  std::string Package::packager() const
  { return lookupStrAttribute( sat::SolvAttr::packager ); }

  std::string Package::group() const
  { return lookupStrAttribute( sat::SolvAttr::group ); }

  Package::Keywords Package::keywords() const
  { return Keywords( sat::SolvAttr::keywords, satSolvable() ); }

  std::string Package::url() const
  { return lookupStrAttribute( sat::SolvAttr::url ); }

  ByteCount Package::sourcesize() const
  { return lookupNumAttribute( sat::SolvAttr::sourcesize ); }

  std::list<std::string> Package::authors() const
  {
    std::list<std::string> ret;
    str::split( lookupStrAttribute( sat::SolvAttr::authors ), std::back_inserter(ret), "\n" );
    return ret;
  }

  Package::FileList Package::filelist() const
  { return FileList( sat::SolvAttr::filelist, satSolvable() ); }

  CheckSum Package::checksum() const
  { return lookupCheckSumAttribute( sat::SolvAttr::checksum ); }

  OnMediaLocation Package::location() const
  { return lookupLocation(); }

  Pathname Package::cachedLocation() const
  {
    const auto &optRepoInfo = ngRepoInfo ();
    if ( !optRepoInfo )
      return Pathname();
    return zyppintern::cachedLocation( location(), *optRepoInfo );
  }

  std::string Package::sourcePkgName() const
  {
    // no id means same as package
    sat::detail::IdType id( lookupIdAttribute( sat::SolvAttr::sourcename ) );
    return id ? IdString( id ).asString() : name();
  }

  Edition Package::sourcePkgEdition() const
  {
   // no id means same as package
    sat::detail::IdType id( lookupIdAttribute( sat::SolvAttr::sourceevr ) );
    return id ? Edition( id ) : edition();
  }

  std::string Package::sourcePkgType() const
  { return lookupStrAttribute( sat::SolvAttr::sourcearch ); }

  std::string Package::sourcePkgLongName() const
  { return str::form( "%s-%s.%s", sourcePkgName().c_str(), sourcePkgEdition().c_str(), sourcePkgType().c_str() ); }


  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
