/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-media/auth/credentialmanager.cc
 *
 */

#include "credentialmanager.h"

#include <zypp-media/MediaConfig>
#include <zypp-media/ng/MediaContext>


using std::endl;

constexpr std::string_view USER_CREDENTIALS_FILE(".zypp/credentials.cat");

namespace zypp
{
  namespace media
  {
    namespace {
      void setUserCredFilePath( const zypp::Pathname &root, zypp::Pathname &userCredFilePath ) {
        char * homedir = getenv("HOME");
        if (homedir)
          userCredFilePath =  root / homedir / USER_CREDENTIALS_FILE.data ();
      }
    }

    CredManagerSettings::CredManagerSettings( zyppng::MediaContextRef ctx )
      : globalCredFilePath( ctx->contextRoot() / ctx->mediaConfig().credentialsGlobalFile() )
      , customCredFileDir ( ctx->contextRoot() / ctx->mediaConfig().credentialsGlobalDir() )
    {
      setUserCredFilePath( ctx->contextRoot(), userCredFilePath );
    }

    CredManagerSettings::CredManagerSettings()
      : globalCredFilePath( MediaConfig::defaults().credentialsGlobalFile() )
      , customCredFileDir ( MediaConfig::defaults().credentialsGlobalDir() )
    {
      setUserCredFilePath( "", userCredFilePath );
    }

    const char *CredManagerSettings::userCredFile()
    {
      return USER_CREDENTIALS_FILE.data();
    }

    ////////////////////////////////////////////////////////////////////
  } // media
  //////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////
} // zypp
//////////////////////////////////////////////////////////////////////
