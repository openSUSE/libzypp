/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/media/MediaNetworkCommonHandler.h
 */
#ifndef ZYPP_MEDIA_MEDIANETWORKCOMMONHANDLER_H
#define ZYPP_MEDIA_MEDIANETWORKCOMMONHANDLER_H

#include <zypp/media/MediaHandler.h>
#include <zypp-curl/TransferSettings>

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace media
  {

    class CredentialManager;

    ///////////////////////////////////////////////////////////////////
    /// \class MediaNetworkCommonHandler
    /// \brief Baseclass for MediaCurl
    ///
    /// Access to commonly used stuff like \ref TransferSettings mainly
    //  to avoid duplicated code.
    ///////////////////////////////////////////////////////////////////
    class MediaNetworkCommonHandler : public MediaHandler
    {
    public:
      MediaNetworkCommonHandler( const MediaUrl &url_r,
                                 const std::vector<MediaUrl> &mirrors_r,
                                 const Pathname & attach_point_r,
                                 const Pathname & urlpath_below_attachpoint_r,
                                 const bool       does_download_r );

    public:
      /**
       * Rewrites the baseURL to the geoIP target if one is found in the metadata cache,
       * otherwise simply returns the url again.
       */
      static zypp::Url findGeoIPRedirect ( const zypp::Url &url );

      void getFile( const OnMediaLocation & file ) const override;
      void getDir( const Pathname & dirname, bool recurse_r ) const override;
      void getDirInfo( std::list<std::string> & retlist,
                               const Pathname & dirname, bool dots = true ) const override;
      void getDirInfo( filesystem::DirContent & retlist,
                               const Pathname & dirname, bool dots = true ) const override;

    protected:

      void attachTo (bool next) override;

      bool checkAttachPoint(const Pathname &apoint) const override;

      Url clearQueryString(const Url &url) const;


      /**
       * concatenate the attach url and the filename to a complete
       * download url
       **/
      Url getFileUrl( int mirrorIdx, const Pathname & filename) const;


      /**
       * initializes the curl easy handle with the data from the url
       * \throws MediaCurlSetOptException if there is a problem
       **/
      void setupTransferSettings();

      bool authenticate(const Url &url, TransferSettings &settings, const std::string & availAuthTypes, bool firstTry );

      /**
       * check the url is supported by the curl library
       * \throws MediaBadUrlException if there is a problem
       **/
      virtual void checkProtocol(const Url &url) const = 0;

    public:

      // standard auth procedure, shared with CommitPackagePreloader
      static bool authenticate( const Url &url, CredentialManager &cm, TransferSettings &settings, const std::string & availAuthTypes, bool firstTry);

      static const char *anonymousIdHeader();

      static const char *distributionFlavorHeader();

      static const char *agentString();

      template <typename Excpt>
      static bool canTryNextMirror( const Excpt &excpt_r ) {
        // check if we can retry on the next mirror
        bool fatal = (
              typeid(excpt_r) == typeid( MediaRequestCancelledException ) ||
              typeid(excpt_r) == typeid( MediaWriteException )            ||
              typeid(excpt_r) == typeid( MediaSystemException )           ||
              typeid(excpt_r) == typeid( MediaCurlInitException )         ||
              typeid(excpt_r) == typeid( MediaUnauthorizedException )
        );
        return !fatal;
      }

    protected:
      std::vector<Url> _redirTargets;
      std::vector<TransferSettings> _mirrorSettings;
    };

  } // namespace media
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_MEDIA_MEDIANETWORKCOMMONHANDLER_H
