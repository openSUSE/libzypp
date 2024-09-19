/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/zypp_detail/urlcredentialextractor_p.h
 *
*/

#ifndef ZYPP_ZYPP_DETAIL_URLCREDENTIALEXTRACTOR_P_H
#define ZYPP_ZYPP_DETAIL_URLCREDENTIALEXTRACTOR_P_H

#include <zypp-core/Url.h>

#include <zypp/ng/context.h>
#include <zypp-media/ng/auth/credentialmanager.h>

namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  /// \class UrlCredentialExtractor
  /// \brief Extract credentials in \ref Url authority and store them via \ref CredentialManager.
  ///
  /// Lazy init CredentialManager and save collected credentials when
  /// going out of scope.
  ///
  /// Methods return whether a password has been collected/extracted.
  ///
  /// \code
  /// UrlCredentialExtractor( "/rootdir" ).collect( oneUrlOrUrlContainer );
  /// \endcode
  /// \code
  /// {
  ///   UrlCredentialExtractor extractCredentials;
  ///   extractCredentials.collect( oneUrlOrUrlContainer );
  ///   extractCredentials.extract( oneMoreUrlOrUrlContainer );
  ///   ....
  /// }
  /// \endcode
  ///
  class UrlCredentialExtractor
  {
  public:
    UrlCredentialExtractor( zyppng::ContextBaseRef ctx )
      : _ctx( std::move(ctx) )
    {}

    ~UrlCredentialExtractor()
    { if ( _cmPtr ) _cmPtr->save(); }

    /** Remember credentials stored in URL authority leaving the password in \a url_r. */
    bool collect( const Url & url_r )
    {
      bool ret = url_r.hasCredentialsInAuthority();
      if ( ret )
      {
        if ( !_cmPtr ) _cmPtr = zyppng::media::CredentialManager::create( media::CredManagerSettings(_ctx) );
        _cmPtr->addUserCred( url_r );
      }
      return ret;
    }
    /** \overload operating on Url container */
    template<class TContainer>
    bool collect( const TContainer & urls_r )
    {	bool ret = false; for ( const Url & url : urls_r ) { if ( collect( url ) && !ret ) ret = true; } return ret; }

    /** Remember credentials stored in URL authority stripping the passowrd from \a url_r. */
    bool extract( Url & url_r )
    {
      bool ret = collect( url_r );
      if ( ret )
        url_r.setPassword( std::string() );
      return ret;
    }
    /** \overload operating on Url container */
    template<class TContainer>
    bool extract( TContainer & urls_r )
    {	bool ret = false; for ( Url & url : urls_r ) { if ( extract( url ) && !ret ) ret = true; } return ret; }

  private:
    zyppng::ContextBaseRef _ctx;
    zyppng::media::CredentialManagerRef _cmPtr;
  };
}

#endif
