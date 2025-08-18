/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_REPO_MIRRORLIST_H_
#define ZYPP_REPO_MIRRORLIST_H_

#include <vector>
#include <zypp-core/Url.h>
#include <zypp-core/Pathname.h>

namespace zypp
{
  namespace repo
  {
    class RepoMirrorList
    {
      public:

        RepoMirrorList( const Url & url_r, const Pathname & metadatapath_r );

        RepoMirrorList( const Url & url_r )
        : RepoMirrorList( url_r, Pathname() )
        {}

        const std::vector<Url> & getUrls() const
        { return _urls; }

        std::vector<Url> & getUrls()
        { return _urls; }

        static bool urlSupportsMirrorLink( const zypp::Url &url );

        constexpr static const char *cookieFileName()
        {
          return "mirrorlist.cookie";
        }

        constexpr static const char* cacheFileName()
        {
          return "mirrorlist";
        }

    private:
        static void saveToCookieFile(const Pathname &path_r, const zypp::Url &url_r );
        static std::string readCookieFile( const Pathname &path_r );
        static std::string makeCookie( const zypp::Url &url_r );

      private:
        std::vector<Url> _urls;
    };
  } // ns repo
} // ns zypp

#endif

// vim: set ts=2 sts=2 sw=2 et ai:
