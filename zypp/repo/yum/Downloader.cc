/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include <zypp/base/String.h>
#include <zypp/base/LogTools.h>
#include <zypp/base/Function.h>
#include <zypp/ZConfig.h>

#include "Downloader.h"
#include "RepomdFileCollector.h"
#include <zypp/repo/MediaInfoDownloader.h>
#include <zypp/repo/SUSEMediaVerifier.h>
#include <zypp-core/base/UserRequestException>
#include <zypp/parser/xml/Reader.h>
#include <zypp/parser/yum/RepomdFileReader.h>

using namespace zypp::xml;
using namespace zypp::parser::yum;

namespace zypp
{
namespace repo
{
namespace yum
{
  ///////////////////////////////////////////////////////////////////
  /// \class Downloader::Impl
  /// \brief Helper filtering the files offered by a RepomdFileReader
  ///
  /// Clumsy construct; basically an Impl class for Downloader, maintained
  /// in Downloader::download only while parsing a repomd.xml.
  ///     File types:
  ///         type        (plain)
  ///         type_db     (sqlite, ignored by zypp)
  ///         type_zck    (zchunk, preferred)
  ///     Localized type:
  ///         susedata.LOCALE
  ///////////////////////////////////////////////////////////////////
  class Downloader::Impl : public RepomdFileCollector
  {
  public:
    NON_COPYABLE( Impl );
    NON_MOVABLE( Impl );

    Impl( Downloader & downloader_r, MediaSetAccess &, const Pathname & destDir_r )
    : RepomdFileCollector( destDir_r )
    , _downloader { downloader_r }
    { }

    void finalize() {
      RepomdFileCollector::finalize ([this]( const OnMediaLocation &loc ){
        _downloader.enqueueDigested( loc, FileChecker() );
      });
    }

  private:
    const RepoInfo &repoInfo() const override
    { return _downloader.repoInfo(); }

    const Pathname & deltaDir() const override
    { return _downloader._deltaDir; }

  private:
    Downloader & _downloader;
  };

  ///////////////////////////////////////////////////////////////////
  //
  // class Downloader
  //
  ///////////////////////////////////////////////////////////////////

  Downloader::Downloader( const RepoInfo & info_r, const Pathname & deltaDir_r )
  : repo::Downloader { info_r}
  , _deltaDir { deltaDir_r }
  {}

  void Downloader::download( MediaSetAccess & media_r, const Pathname & destDir_r, const ProgressData::ReceiverFnc & progress_r )
  {
    downloadMediaInfo( destDir_r, media_r );

    Pathname masterIndex { repoInfo().path() / "/repodata/repomd.xml" };
    defaultDownloadMasterIndex( media_r, destDir_r, masterIndex );

    //enable precache
    setMediaSetAccess( media_r );

    // setup parser
    Impl pimpl( *this, media_r, destDir_r );
    RepomdFileReader( destDir_r / masterIndex, [&pimpl]( const OnMediaLocation & loc_r, const std::string & typestr_r ){ return pimpl.collect( loc_r, typestr_r ); } );
    pimpl.finalize();

    // ready, go!
    start( destDir_r );
  }

  RepoStatus Downloader::status( MediaSetAccess & media_r )
  {
    const auto & ri = repoInfo();
    RepoStatus ret { media_r.provideOptionalFile( ri.path() / "/repodata/repomd.xml" ) };
    if ( !ret.empty() && ri.requireStatusWithMediaFile() )	// else: mandatory master index is missing
      ret = ret && RepoStatus( media_r.provideOptionalFile( "/media.1/media" ) );
    // else: mandatory master index is missing -> stay empty
    return ret;
  }
} // namespace yum
} // namespace repo
} // namespace zypp
