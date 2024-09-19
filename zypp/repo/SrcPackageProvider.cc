/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/SrcPackageProvider.cc
 *
*/
#include <iostream>
#include <fstream>

#include <zypp/base/Gettext.h>
#include <zypp/repo/SrcPackageProvider.h>
#include <zypp/PathInfo.h>
#include <zypp/TmpPath.h>
#include <zypp/SrcPackage.h>
#include <zypp/ng/repoinfo.h>
#include <zypp/repo/RepoException.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace repo
  {
    SrcPackageProvider::SrcPackageProvider( repo::RepoMediaAccess & access_r )
      : _access( access_r )
    {}

    SrcPackageProvider::~SrcPackageProvider()
    {}

    ManagedFile SrcPackageProvider::provideSrcPackage( const SrcPackage_constPtr & srcPackage_r ) const
    {
      const auto &ri = srcPackage_r->ngRepoInfo();
      if ( !ri ) {
        RepoException repo_excpt(str::form(_("Can't provide file '%s' without a RepoInfo"),
                                 srcPackage_r->location().filename().c_str() ) );

        ZYPP_THROW(repo_excpt);
      }
      return _access.provideFile( *ri, srcPackage_r->location() );
    }

  } // namespace repo
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
