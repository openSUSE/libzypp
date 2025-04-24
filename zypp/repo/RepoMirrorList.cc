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
#include <utility>
#include <vector>
#include <time.h>
#include <zypp/repo/RepoMirrorList.h>
#include <zypp-curl/parser/MetaLinkParser>
#include <zypp/MediaSetAccess.h>
#include <zypp/base/LogTools.h>
#include <zypp/ZConfig.h>
#include <zypp/PathInfo.h>
#include <zypp-core/base/Exception.h>

#include <zypp-core/fs/TmpPath.h>
#include <zypp-curl/ng/network/networkrequestdispatcher.h>
#include <zypp-curl/ng/network/request.h>
#include <zypp-curl/private/curlhelper_p.h>
#include <zypp-core/zyppng/base/eventloop.h>
#include <zypp-core/zyppng/pipelines/expected.h>

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
          _access.reset( new MediaSetAccess( std::vector<zypp::media::MediaUrl>{abs_url} ) );
          _localfile = _access->provideFile( url_r.getPathName() );

        }

        const Pathname & localfile() const
        { return _localfile; }

      private:
        shared_ptr<MediaSetAccess> _access;
        Pathname _localfile;
        std::optional<filesystem::TmpFile> _tmpfile;
      };
      ///////////////////////////////////////////////////////////////////

      inline std::vector<Url> RepoMirrorListParseXML( const Pathname &tmpfile )
      {
        InputStream tmpfstream (tmpfile);
        media::MetaLinkParser metalink;
        metalink.parse(tmpfstream);
        return metalink.getUrls();
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
            if ( data.type() != json::Value::ArrayType ) {
              MIL << "Unexpected JSON format, top level element must be an array." << std::endl;
              return zyppng::expected<std::vector<Url>>::error( ZYPP_EXCPT_PTR( zypp::Exception("Unexpected JSON format, top level element must be an array.") ));
            }

            std::vector<Url> urls;
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
            return {};
          }

          return *res;

        } catch (...) {
          MIL << "Caught exception while parsing json, no mirrors available" << std::endl;
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
            my_urls.push_back(Url(tmpurl));
          }
          catch (...)
          {;}	// ignore malformed urls
        }
        return my_urls;
      }

      /** Parse a local mirrorlist \a listfile_r and return usable URLs */
      inline std::vector<Url> RepoMirrorListParse( const Url & url_r, const Pathname & listfile_r, RepoMirrorList::Format format )
      {
        USR << url_r << " " << listfile_r << endl;

        std::vector<Url> mirrorurls;
        if ( format == RepoMirrorList::MetaLink || url_r.asString().find( "/metalink" ) != std::string::npos )
          mirrorurls = RepoMirrorListParseXML( listfile_r );
        else if ( format == RepoMirrorList::MirrorListJson || url_r.getQueryStringMap().count("mirrorlist") != 0 )
          mirrorurls = RepoMirrorListParseJSON( listfile_r );
        else
          mirrorurls = RepoMirrorListParseTXT( listfile_r );


        std::vector<Url> ret;
        for ( auto & murl : mirrorurls )
        {
          if ( murl.getScheme() != "rsync" )
          {
            size_t delpos = murl.getPathName().find("repodata/repomd.xml");
            if( delpos != std::string::npos )
            {
              murl.setPathName( murl.getPathName().erase(delpos)  );
            }
            ret.push_back( murl );
          }
        }
        return ret;
      }

    } // namespace
    ///////////////////////////////////////////////////////////////////

    RepoMirrorList::RepoMirrorList( const Url & url_r, const Pathname & metadatapath_r, Format format )
    {

      PathInfo metaPathInfo( metadatapath_r);
      if ( url_r.getScheme() == "file" )
      {
        // never cache for local mirrorlist
        _urls = RepoMirrorListParse( url_r, url_r.getPathName(), format );
      }
      else if ( !metaPathInfo.isDir() )
      {
        // no cachedir or no access
        RepoMirrorListTempProvider provider( url_r );	// RAII: lifetime of any downloaded files
        _urls = RepoMirrorListParse( url_r, provider.localfile(), format );
      }
      else
      {
        // have cachedir
        Pathname cachefile( metadatapath_r );
        if ( format == MetaLink || url_r.asString().find( "/metalink" ) != std::string::npos )
          cachefile /= "mirrorlist.xml";
        else if ( format == MirrorListJson || url_r.getQueryStringMap().count("mirrorlist") != 0 )
          cachefile /= "mirrorlist.json";
        else
          cachefile /= "mirrorlist.txt";

        zypp::filesystem::PathInfo cacheinfo( cachefile );
        if ( !cacheinfo.isFile()
             // force a update on a old cache ONLY if the user can write the cache, otherwise we use an already existing cachefile
             // it makes no sense to continously download the mirrors file if we can't store it
             || ( cacheinfo.mtime() < time(NULL) - (long) ZConfig::instance().repo_refresh_delay() * 60 && metaPathInfo.userMayRWX () )
        )
        {
          DBG << "Getting MirrorList from URL: " << url_r << endl;
          RepoMirrorListTempProvider provider( url_r );	// RAII: lifetime of downloaded file

          if ( metaPathInfo.userMayRWX () ) {
            // Create directory, if not existing
            DBG << "Copy MirrorList file to " << cachefile << endl;
            zypp::filesystem::assert_dir( metadatapath_r );
            zypp::filesystem::hardlinkCopy( provider.localfile(), cachefile );
          }
          _urls = RepoMirrorListParse( url_r, provider.localfile(), format );

        } else {
          _urls = RepoMirrorListParse( url_r, cachefile, format );
        }


        if( _urls.empty() )
        {
          DBG << "Removing Cachefile as it contains no URLs" << endl;
          zypp::filesystem::unlink( cachefile );
        }
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

    /////////////////////////////////////////////////////////////////
  } // namespace repo
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
