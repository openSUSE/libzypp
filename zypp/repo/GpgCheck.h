/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/source/GpgCheck.h
 *
*/
#ifndef ZYPP_REPO_GPGCHECK_H_INCLUDED
#define ZYPP_REPO_GPGCHECK_H_INCLUDED

#include <ostream>
#include <zypp/Globals.h>

namespace zypp {
  namespace repo {
    /** Some predefined settings */
    enum class ZYPP_API GpgCheck {
      indeterminate,		//< not specified
      On,			//< 1** --gpgcheck
      Strict,			//< 111 --gpgcheck-strict
      AllowUnsigned,		//< 100 --gpgcheck-allow-unsigned
      AllowUnsignedRepo,	//< 10* --gpgcheck-allow-unsigned-repo
      AllowUnsignedPackage,	//< 1*0 --gpgcheck-allow-unsigned-package
      Default,		//< *** --default-gpgcheck
      Off,			//< 0** --no-gpgcheck
    };
  }

  /** \relates RepoInfo::GpgCheck Stream output */
  std::ostream & operator<<( std::ostream & str, const repo::GpgCheck & obj ) ZYPP_API;
}




#endif //ZYPP_REPO_GPGCHECK_H_INCLUDED
