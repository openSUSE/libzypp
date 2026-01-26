/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/RepoMirrorList.cc
 *
*/
#include "zypp-media/auth/credentialmanager.h"
#include <iostream>
#include <fstream>
#include <utility>
#include <vector>
#include <time.h>
#include <zypp/repo/RepoMirrorList.h>
#include <zypp-curl/parser/MetaLinkParser>
#include <zypp/MediaSetAccess.h>
#include <zypp-core/base/LogTools.h>
#include <zypp/ZConfig.h>
#include <zypp/PathInfo.h>
#include <zypp-core/base/Exception.h>

#include <zypp-core/fs/TmpPath.h>
#include <zypp-curl/ng/network/networkrequestdispatcher.h>
#include <zypp-curl/ng/network/request.h>
#include <zypp-curl/private/curlhelper_p.h>
#include <zypp-core/ng/base/eventloop.h>
#include <zypp-core/ng/pipelines/expected.h>

#include <zypp/media/detail/MediaNetworkRequestExecutor.h>
#include <zypp/media/MediaNetworkCommonHandler.h> // for the authentication workflow

#include <zypp-core/parser/json.h>


///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace repo
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    namespace
    {
      ///////////////////////////////////////////////////////////////////
      /// \class RepoMirrorListTempProvider
      /// \brief Provide access to downloaded mirror list (in temp space)
      /// \ingroup g_RAII
      ///
      /// Tempspace (and mirror list) are deleted when provider goes out
      /// of scope.
      struct RepoMirrorListTempProvider
      {
        RepoMirrorListTempProvider()
        {}

        RepoMirrorListTempProvider( Pathname localfile_r )
        : _localfile(std::move( localfile_r ))
        {}

        RepoMirrorListTempProvider( const Url & url_r )
        {
          if ( url_r.schemeIsDownloading()
               && RepoMirrorList::urlSupportsMirrorLink(url_r)
               && url_r.getQueryStringMap().count("mirrorlist") > 0 ) {

            // Auth will probably never be triggered, but add it for completeness
            const auto &authCb = [&]( const zypp::Url &, media::TransferSettings &settings, const std::string & availAuthTypes, bool firstTry, bool &canContinue ) {
              media::CredentialManager cm(media::CredManagerOptions(ZConfig::instance().repoManagerRoot()));
              if ( media::MediaNetworkCommonHandler::authenticate( url_r, cm, settings, availAuthTypes, firstTry ) ) {
                canContinue = true;
                return;
              }
              canContinue = false;
            };

            internal::MediaNetworkRequestExecutor executor;
            executor.sigAuthRequired ().connect(authCb);

            _tmpfile = filesystem::TmpFile();
            _localfile = _tmpfile->path();

            // prepare Url and Settings
            auto url = url_r;
            auto tSettings = media::TransferSettings();
            ::internal::prepareSettingsAndUrl( url, tSettings );

            auto req = std::make_shared<zyppng::NetworkRequest>( url_r, _localfile );
            req->transferSettings () = tSettings;
            executor.executeRequest ( req, nullptr );

            // apply umask
            if ( ::chmod( _localfile.c_str(), filesystem::applyUmaskTo( 0644 ) ) )
            {
              ERR << "Failed to chmod file " << _localfile << endl;
            }

            return;
          }

          // this will handle traditional media including URL resolver plugins
          Url abs_url( url_r );
          abs_url.setPathName( "/" );
          _access.reset( new MediaSetAccess( zypp::MirroredOrigin{abs_url} ) );
          _localfile = _access->provideFile( url_r.getPathName() );

        }

        const Pathname & localfile() const
        { return _localfile; }
      private:
        shared_ptr<MediaSetAccess> _access;
        Pathname _localfile;
        std::optional<filesystem::TmpFile> _tmpfile;
      };

      enum class RepoMirrorListFormat {
        Error,
        Empty,
        MirrorListTxt,
        MirrorListJson,
        MetaLink
      };

      static RepoMirrorListFormat detectRepoMirrorListFormat( const Pathname &localfile ) {
        // a file starting with < is most likely a metalink file,
        // a file starting with [ is most likely a json file,
        // else we go for txt
        MIL << "Detecting RepoMirrorlist Format based on file content" << std::endl;

        if ( localfile.empty () )
          return RepoMirrorListFormat::Empty;

        InputStream tmpfstream (localfile);
        auto &str = tmpfstream.stream();
        auto c = str.get ();

        // skip preceding whitespaces
        while ( !str.eof () && !str.bad() && ( c == ' ' || c == '\t' || c == '\n' || c == '\r') )
          c = str.get ();

        if ( str.eof() ) {
          ERR << "Failed to read RepoMirrorList file, stream hit EOF early." << std::endl;
          return RepoMirrorListFormat::Empty;
        }

        if ( str.bad() ) {
          ERR << "Failed to read RepoMirrorList file, stream became bad." << std::endl;
          return RepoMirrorListFormat::Error;
        }

        switch ( c ) {
          case '<': {
            MIL << "Detected Metalink, file starts with <" << std::endl;
            return RepoMirrorListFormat::MetaLink;
          }
          case '[': {
            MIL << "Detected JSON, file starts with [" << std::endl;
            return RepoMirrorListFormat::MirrorListJson;
          }
          default: {
            MIL << "Detected TXT, file starts with " << c << std::endl;
            return RepoMirrorListFormat::MirrorListTxt;
          }
        }
      }

      inline std::vector<Url> RepoMirrorListParseXML( const Pathname &tmpfile )
      {
        try {
          media::MetaLinkParser metalink;
          metalink.parse(tmpfile);
          return metalink.getUrls();
        } catch (...) {
          ZYPP_CAUGHT( std::current_exception() );
          zypp::parser::ParseException ex("Invalid repo metalink format.");
          ex.remember ( std::current_exception () );
          ZYPP_THROW(ex);
        }
      }

      inline std::vector<Url> RepoMirrorListParseJSON( const Pathname &tmpfile )
      {
        InputStream tmpfstream (tmpfile);

        try {
          using namespace zyppng::operators;
          using zyppng::operators::operator|;

          json::Parser parser;
          auto res = parser.parse ( tmpfstream )
          | and_then([&]( json::Value data ) {

            std::vector<Url> urls;
            if ( data.isNull () ) {
              MIL << "Empty mirrorlist received, no mirrors available." << std::endl;
              return zyppng::make_expected_success(urls);
            }

            if ( data.type() != json::Value::ArrayType ) {
              MIL << "Unexpected JSON format, top level element must be an array." << std::endl;
              return zyppng::expected<std::vector<Url>>::error( ZYPP_EXCPT_PTR( zypp::Exception("Unexpected JSON format, top level element must be an array.") ));
            }
            const auto &topArray = data.asArray ();
            for ( const auto &val : topArray ) {
              if ( val.type () != json::Value::ObjectType ) {
                MIL << "Unexpected JSON element, array must contain only objects. Ignoring current element" << std::endl;
                continue;
              }

              const auto &obj = val.asObject();
              for ( const auto &key : obj ) {
                if ( key.first == "url" ) {
                  const auto &elemValue = key.second;
                  if ( elemValue.type() != json::Value::StringType ) {
                    MIL << "Unexpected JSON element, element \"url\" must contain a string. Ignoring current element" << std::endl;
                    break;
                  }
                  try {
                    MIL << "Trying to parse URL: " << std::string(elemValue.asString()) << std::endl;
                    urls.push_back ( Url( elemValue.asString() ) );
                  } catch ( const url::UrlException &e ) {
                    ZYPP_CAUGHT(e);
                    MIL << "Invalid URL in mirrors file: "<< elemValue.asString() << ", ignoring" << std::endl;
                  }
                }
              }
            }
            return zyppng::make_expected_success(urls);
          });

          if ( !res ) {
            using zypp::operator<<;
            MIL << "Error while parsing mirrorlist: (" << res.error() << "), no mirrors available" << std::endl;
            ZYPP_RETHROW( res.error () );
          }

          return *res;

        } catch (...) {
          ZYPP_CAUGHT( std::current_exception() );
          MIL << "Caught exception while parsing json" << std::endl;

          zypp::parser::ParseException ex("Invalid repo mirror list format, valid JSON was expected.");
          ex.remember ( std::current_exception () );
          ZYPP_THROW(ex);
        }
        return {};
      }

      inline std::vector<Url> RepoMirrorListParseTXT( const Pathname &tmpfile )
      {
        InputStream tmpfstream (tmpfile);
        std::vector<Url> my_urls;
        std::string tmpurl;
        while (getline(tmpfstream.stream(), tmpurl))
        {
          if ( tmpurl[0] == '#' )
            continue;
          try {
            Url mirrUrl( tmpurl );
            if ( !mirrUrl.schemeIsDownloading( ) ) {
              MIL << "Ignoring non downloading URL " << tmpurl << std::endl;
            }
            my_urls.push_back(Url(tmpurl));
          }
          catch (...)
          {
            ZYPP_CAUGHT( std::current_exception() );

            // fail on invalid URLs
            ERR << "Invalid URL in mirrorlist file." << std::endl;

            zypp::parser::ParseException ex("Invalid repo mirror list format, all Urls must be valid in a mirrorlist txt file.");
            ex.remember ( std::current_exception () );
            ZYPP_THROW(ex);
          }
        }
        return my_urls;
      }

      /** Parse a local mirrorlist \a listfile_r and return usable URLs */
      inline std::vector<Url> RepoMirrorListParse( const Url & url_r, const Pathname & listfile_r )
      {
        MIL << "Parsing mirrorlist file: " << listfile_r << " originally received from " << url_r << endl;

        std::vector<Url> mirrorurls;
        switch( detectRepoMirrorListFormat (listfile_r) ) {
          case RepoMirrorListFormat::Error:
            // should not happen, except when the instr goes bad
            ZYPP_THROW( zypp::parser::ParseException( str::Format("Unable to detect metalink file format for: %1%") % listfile_r ));
          case RepoMirrorListFormat::Empty:
            mirrorurls = {};
            break;
          case RepoMirrorListFormat::MetaLink:
            mirrorurls = RepoMirrorListParseXML( listfile_r );
            break;
          case RepoMirrorListFormat::MirrorListJson:
            mirrorurls = RepoMirrorListParseJSON( listfile_r );
            break;
          case RepoMirrorListFormat::MirrorListTxt:
            mirrorurls = RepoMirrorListParseTXT( listfile_r );
            break;
        }

        std::vector<Url> ret;
        for ( auto & murl : mirrorurls )
        {
          if ( murl.getScheme() != "rsync" )
          {
            std::string pName = murl.getPathName();
            size_t delpos = pName.find("repodata/repomd.xml");
            if( delpos != std::string::npos )
            {
              murl.setPathName( pName.erase(delpos)  );
            }
            ret.push_back( murl );
          }
        }
        return ret;
      }

    } // namespace
    ///////////////////////////////////////////////////////////////////

    RepoMirrorList::RepoMirrorList( const Url & url_r, const Pathname & metadatapath_r )
    {
      PathInfo metaPathInfo( metadatapath_r);
      std::exception_ptr errors; // we collect errors here
      try {
        if ( url_r.getScheme() == "file" )
        {
          // never cache for local mirrorlist
          _urls = RepoMirrorListParse( url_r, url_r.getPathName() );
        }
        else if ( !metaPathInfo.isDir() )
        {
          // no cachedir or no access
          RepoMirrorListTempProvider provider( url_r );	// RAII: lifetime of any downloaded files
          _urls = RepoMirrorListParse( url_r, provider.localfile() );
        }
        else
        {
          // have cachedir
          const Pathname cachefile  = metadatapath_r / cacheFileName();
          const Pathname cookiefile = metadatapath_r / cookieFileName();
          zypp::filesystem::PathInfo cacheinfo( cachefile );

          bool needRefresh = ( !cacheinfo.isFile()
                          // force a update on a old cache ONLY if the user can write the cache, otherwise we use an already existing cachefile
                          // it makes no sense to continously download the mirrors file if we can't store it
                          || ( cacheinfo.mtime() < time(NULL) - (long) ZConfig::instance().repo_refresh_delay() * 60 && metaPathInfo.userMayRWX () ) )
                          || ( makeCookie( url_r ) != readCookieFile( cookiefile ) );

          // up to date: try to parse and use the URLs if sucessful
          // otherwise fetch the URL again
          if ( !needRefresh ) {
            MIL << "Mirror cachefile cookie valid and cache is not too old, skipping download (" << cachefile << ")" << std::endl;
            try {
              _urls = RepoMirrorListParse( url_r, cachefile );
              return;
            } catch ( const zypp::Exception & e ) {
              ZYPP_CAUGHT(e);
              auto ex = e;
              if ( errors )
                ex.remember(errors);
              errors = std::make_exception_ptr(ex);
              MIL << "Invalid mirrorlist cachefile, deleting it and trying to fetch a new one" << std::endl;
            }
          }

          // remove the old cache and its cookie, it's either broken, empty or outdated
          if( cacheinfo.isFile() ) {
            filesystem::unlink(cachefile);
          }

          if ( zypp::filesystem::PathInfo(cookiefile).isFile() ) {
            filesystem::unlink(cookiefile);
          }

          MIL << "Getting MirrorList from URL: " << url_r << endl;
          RepoMirrorListTempProvider provider( url_r );	// RAII: lifetime of downloaded file
          _urls = RepoMirrorListParse( url_r, provider.localfile() );

          // removed the && !_urls.empty() condition , we need to remember "no URLs" as well
          // otherwise RepoInfo keeps spamming the server with requests
          if ( metaPathInfo.userMayRWX() ) {
            // Create directory, if not existing
            DBG << "Copy MirrorList file to " << cachefile << endl;
            zypp::filesystem::assert_dir( metadatapath_r );
            if( zypp::filesystem::hardlinkCopy( provider.localfile(), cachefile ) != 0 ) {
              // remember empty file
              zypp::filesystem::assert_file( cachefile );
            }
            saveToCookieFile ( cookiefile, url_r );
            // NOTE: Now we copied the mirrorlist into the metadata directory, but
            // in case of refresh going on, new metadata are prepared in a sibling
            // temp dir. Upon success RefreshContext<>::saveToRawCache() exchanges
            // temp and metadata dirs. There we move an existing mirrorlist file into
            // the new metadata dir.
          }
        }
      } catch ( const zypp::Exception &e ) {
        // Make a more user readable exception
        ZYPP_CAUGHT(e);
        parser::ParseException ex( str::Format("Failed to parse/receive mirror information for URL: %1%") % url_r );
        ex.remember(e);
        if ( errors ) ex.remember(errors);
        ZYPP_THROW(ex);
      }
    }

    bool RepoMirrorList::urlSupportsMirrorLink(const Url &url)
    {
      static const std::vector<std::string> hosts{
        "download.opensuse.org",
        "cdn.opensuse.org"
      };
      return ( std::find( hosts.begin(), hosts.end(), str::toLower( url.getHost() )) != hosts.end() );
    }

    std::string RepoMirrorList::readCookieFile(const Pathname &path_r)
    {
      std::ifstream file( path_r.c_str() );
      if ( not file ) {
        WAR << "No cookie file " << path_r << endl;
        return {};
      }

      return str::getline( file );
    }

    /**
     * Generates the cookie value, currently this is only derived from the Url.
     */
    std::string RepoMirrorList::makeCookie( const Url &url_r )
    {
      return CheckSum::sha256FromString( url_r.asCompleteString() ).checksum();
    }

    void RepoMirrorList::saveToCookieFile(const Pathname &path_r, const Url &url_r )
    {
      std::ofstream file(path_r.c_str());
      if (!file) {
        ERR << str::Str() << "Can't open " << path_r.asString() << std::endl;
        return;
      }
      MIL << "Saving mirrorlist cookie file " << path_r << std::endl;
      file << makeCookie(url_r);
      file.close();
    }

    /////////////////////////////////////////////////////////////////
  } // namespace repo
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
