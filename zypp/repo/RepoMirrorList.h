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
#include <zypp/Url.h>
#include <zypp/Pathname.h>

namespace zypp
{
  namespace repo
  {
    class RepoMirrorList
    {
      public:

      enum Format {
        Default,
        MirrorListTxt,
        MirrorListJson,
        MetaLink
      };

        RepoMirrorList( const Url & url_r, const Pathname & metadatapath_r, Format = Default );

        RepoMirrorList( const Url & url_r )
        : RepoMirrorList( url_r, Pathname() )
        {}

        const std::vector<Url> & getUrls() const
        { return _urls; }

        std::vector<Url> & getUrls()
        { return _urls; }

        static bool urlSupportsMirrorLink( const zypp::Url &url );

      private:
        std::vector<Url> _urls;
    };
  } // ns repo
} // ns zypp

#endif

// vim: set ts=2 sts=2 sw=2 et ai:
