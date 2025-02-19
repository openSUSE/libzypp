/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaCurl2.h
 *
*/
#ifndef ZYPP_MEDIA_MEDIACURL2_H
#define ZYPP_MEDIA_MEDIACURL2_H

#include <zypp-core/zyppng/base/zyppglobal.h>
#include <zypp/base/Flags.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/media/MediaNetworkCommonHandler.h>

#include <curl/curl.h>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS (EventDispatcher);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (NetworkRequestDispatcher);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (NetworkRequest);
}

namespace zypp {
  namespace media {

  class CredentialManager;

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : MediaCurl2
/**
 * @short Implementation class for FTP, HTTP and HTTPS MediaHandler
 * @see MediaHandler
 **/
class MediaCurl2 : public MediaNetworkCommonHandler
{
  public:
    enum RequestOption
    {
        /** Defaults */
        OPTION_NONE = 0x0,
        /** retrieve only a range of the file */
        OPTION_RANGE = 0x1,
        /** only issue a HEAD (or equivalent) request */
        OPTION_HEAD = 0x02,
        /** to not add a IFMODSINCE header if target exists */
        OPTION_NO_IFMODSINCE = 0x04,
        /** do not send a start ProgressReport */
        OPTION_NO_REPORT_START = 0x08,
    };
    ZYPP_DECLARE_FLAGS(RequestOptions,RequestOption);

  protected:

    Url clearQueryString(const Url &url) const;

    void attachTo (bool next = false) override;
    void releaseFrom( const std::string & ejectDev ) override;
    void getFile( const OnMediaLocation & file ) const override;
    void getDir( const Pathname & dirname, bool recurse_r ) const override;
    void getDirInfo( std::list<std::string> & retlist,
                             const Pathname & dirname, bool dots = true ) const override;
    void getDirInfo( filesystem::DirContent & retlist,
                             const Pathname & dirname, bool dots = true ) const override;
    /**
     * Repeatedly calls doGetDoesFileExist() until it successfully returns,
     * fails unexpectedly, or user cancels the operation. This is used to
     * handle authentication or similar retry scenarios on media level.
     */
    bool getDoesFileExist( const Pathname & filename ) const override;

    /**
     *
     * \throws MediaException
     *
     */
    void disconnectFrom() override;
    /**
     *
     * \throws MediaException
     *
     */
    void getFileCopy( const OnMediaLocation& srcFile, const Pathname & targetFilename ) const override;

    /**
     *
     * \throws MediaException
     *
     */
    virtual void doGetFileCopy( const OnMediaLocation &srcFile, const Pathname & targetFilename, callback::SendReport<DownloadProgressReport> & _report,  RequestOptions options = OPTION_NONE ) const;


    bool checkAttachPoint(const Pathname &apoint) const override;

  public:

    MediaCurl2( const Url &      url_r,
               const Pathname & attach_point_hint_r );

    ~MediaCurl2() override { try { release(); } catch(...) {} }

    // standard auth procedure, shared with CommitPackagePreloader
    static bool authenticate( const Url &url, CredentialManager &cm, TransferSettings &settings, const std::string & availAuthTypes, bool firstTry);

  protected:
    /**
     * check the url is supported by the curl library
     * \throws MediaBadUrlException if there is a problem
     **/
    void checkProtocol(const Url &url) const;

    /**
     * initializes the curl easy handle with the data from the url
     * \throws MediaCurlSetOptException if there is a problem
     **/
    void setupEasy();

  private:
    void executeRequest( zyppng::NetworkRequestRef req, callback::SendReport<DownloadProgressReport> *report = nullptr );

    bool authenticate(const std::string & availAuthTypes, bool firstTry);

    bool tryZchunk( zyppng::NetworkRequestRef req, const OnMediaLocation &srcFile , const Pathname & target, callback::SendReport<DownloadProgressReport> & report  );

  private:
    zyppng::EventDispatcherRef _evDispatcher; //< keep the ev dispatcher alive as long as MediaCurl2 is
    zyppng::NetworkRequestDispatcherRef _nwDispatcher; //< keep the dispatcher alive as well
    TransferSettings _effectiveSettings; // use another level of indirection, _settings contains the user modified settings
};
ZYPP_DECLARE_OPERATORS_FOR_FLAGS(MediaCurl2::RequestOptions);

///////////////////////////////////////////////////////////////////

  } // namespace media
} // namespace zypp

#endif // ZYPP_MEDIA_MEDIACURL2_H
