/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-media/auth/CredentialManager
 *
 */
#ifndef ZYPP_MEDIA_AUTH_CREDENTIALMANAGER_H
#define ZYPP_MEDIA_AUTH_CREDENTIALMANAGER_H

#include <zypp-core/Globals.h>
#include <zypp-core/Pathname.h>
#include <zypp-media/auth/AuthData>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS (MediaContext);
}

namespace zypp
{
  class Url;
  namespace media
  {
  //////////////////////////////////////////////////////////////////////
  //
  // CLASS NAME : CredManagerOptions
  //
  /**
   * \todo configurable cred file locations
   */
  struct ZYPP_API CredManagerSettings
  {
    CredManagerSettings( zyppng::MediaContextRef ctx );
    CredManagerSettings() ZYPP_LOCAL; // to support legacy API and tests
    Pathname globalCredFilePath;
    Pathname userCredFilePath;
    Pathname customCredFileDir;

    static const char *userCredFile();
  };

  } // media
} // zypp

#endif /* ZYPP_MEDIA_AUTH_CREDENTIALMANAGER_H */
