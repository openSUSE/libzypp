/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "susetags.h"
#include "zypp-core/base/Regex.h"
#include <zypp-core/zyppng/ui/ProgressObserver>
#include <zypp-media/ng/ProvideSpec>
#include <zypp/ng/Context>

#include <zypp-core/parser/ParseException>

#include <zypp/parser/susetags/ContentFileReader.h>
#include <zypp/parser/susetags/RepoIndex.h>

#include <zypp/ng/workflows/logichelpers.h>
#include <zypp/ng/workflows/contextfacade.h>
#include <zypp/ng/repo/workflows/repodownloaderwf.h>
#include <zypp/ng/workflows/checksumwf.h>

namespace zyppng::SuseTagsWorkflows {

  namespace {

    using namespace zyppng::operators;

    /*!
     * Implementation of the susetags status calculation logic
     *
     * \TODO this is basically the same as the RPMMD workflow with changed filenames, can we unite them?
     */
    template<class Executor, class OpType>
    struct StatusLogic : public LogicBase<Executor, OpType>{
      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

    public:

      using DlContextRefType = std::conditional_t<zyppng::detail::is_async_op_v<OpType>, repo::AsyncDownloadContextRef, repo::SyncDownloadContextRef>;
      using ZyppContextType = typename DlContextRefType::element_type::ContextType;
      using ProvideType     = typename ZyppContextType::ProvideType;
      using MediaHandle     = typename ProvideType::MediaHandle;
      using ProvideRes      = typename ProvideType::Res;

      StatusLogic( DlContextRefType ctx, MediaHandle &&media  )
        : _ctx(std::move(ctx))
        , _handle(std::move(media))
      {}

      MaybeAsyncRef<expected<zypp::RepoStatus>> execute() {
        return _ctx->zyppContext()->provider()->provide( _handle, _ctx->repoInfo().path() / "content" , ProvideFileSpec() )
          | [this]( expected<ProvideRes> contentFile ) {

              // mandatory master index is missing -> stay empty
              if ( !contentFile )
                return makeReadyResult( make_expected_success (zypp::RepoStatus() ));

              zypp::RepoStatus status ( contentFile->file() );

              if ( !status.empty() /* && _ctx->repoInfo().requireStatusWithMediaFile() */ ) {
                return _ctx->zyppContext()->provider()->provide( _handle, "/media.1/media"  , ProvideFileSpec())
                  | [status = std::move(status)]( expected<ProvideRes> mediaFile ) mutable {
                      if ( mediaFile ) {
                        return make_expected_success(status && zypp::RepoStatus( mediaFile->file()) );
                      }
                      return make_expected_success( std::move(status) );
                    };
              }
              return makeReadyResult( make_expected_success(std::move(status)) );
            };
      }

      DlContextRefType _ctx;
      MediaHandle _handle;
    };
  }

  AsyncOpRef<expected<zypp::RepoStatus> > repoStatus(repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle)
  {
    return SimpleExecutor< StatusLogic, AsyncOp<expected<zypp::RepoStatus>> >::run( std::move(dl), std::move(mediaHandle) );
  }

  expected<zypp::RepoStatus> repoStatus(repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle)
  {
    return SimpleExecutor< StatusLogic, SyncOp<expected<zypp::RepoStatus>> >::run( std::move(dl), std::move(mediaHandle) );
  }



  namespace {

    using namespace zyppng::operators;

    // search old repository file file to run the delta algorithm on
    static zypp::Pathname search_deltafile( const zypp::Pathname &dir, const zypp::Pathname &file )
    {
      zypp::Pathname deltafile(dir + file.basename());
      if (zypp::PathInfo(deltafile).isExist())
        return deltafile;
      return zypp::Pathname();
    }


    template<class Executor, class OpType>
    struct DlLogic : public LogicBase<Executor, OpType> {

      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);
    public:

      using DlContextRefType = std::conditional_t<zyppng::detail::is_async_op_v<OpType>, repo::AsyncDownloadContextRef, repo::SyncDownloadContextRef>;
      using ZyppContextType = typename DlContextRefType::element_type::ContextType;
      using ProvideType     = typename ZyppContextType::ProvideType;
      using MediaHandle     = typename ProvideType::MediaHandle;
      using ProvideRes      = typename ProvideType::Res;

      DlLogic( DlContextRefType ctx, MediaHandle &&mediaHandle, ProgressObserverRef &&progressObserver )
        :  _ctx( std::move(ctx))
        , _mediaHandle(std::move(mediaHandle))
        , _progressObserver(std::move(progressObserver))
      {}

      auto execute() {
        // download media file here
        return RepoDownloaderWorkflow::downloadMediaInfo( _mediaHandle, _ctx->destDir() )
          | [this]( expected<zypp::ManagedFile> &&mediaInfo ) {

              // remember the media info if we had one
              if ( mediaInfo ) _ctx->files().push_back ( std::move(mediaInfo.get()) );

              if ( _progressObserver ) _progressObserver->inc();

              return RepoDownloaderWorkflow::downloadMasterIndex( _ctx, _mediaHandle, _ctx->repoInfo().path() / "content" )
                | inspect( incProgress( _progressObserver ) )
                | and_then( [this] ( DlContextRefType && ) {

                    zypp::Pathname contentPath = _ctx->files().front();
                    std::vector<zypp::OnMediaLocation> requiredFiles;

                    // Content file first to get the repoindex
                    try {
                      const zypp::Pathname &inputfile = contentPath;
                      zypp::parser::susetags::ContentFileReader content;
                      content.setRepoIndexConsumer( [this]( const auto & data_r ) {
                        MIL << "Consuming repo index" << std::endl;
                        _repoindex = data_r;
                      });
                      content.parse( inputfile );

                      if ( ! _repoindex ) {
                        ZYPP_THROW( zypp::parser::ParseException( (  _ctx->destDir() / _ctx->repoInfo().path() ).asString() + ": " + "No repository index in content file." ) );
                      }

                      MIL << "RepoIndex: " << _repoindex << std::endl;
                      if ( _repoindex->metaFileChecksums.empty() ) {
                        ZYPP_THROW( zypp::parser::ParseException( (  _ctx->destDir() / _ctx->repoInfo().path() ).asString() +  ": " + "No metadata checksums in content file." ) );
                      }

                      if ( _repoindex->signingKeys.empty() ) {
                        WAR << "No signing keys defined." << std::endl;
                      }

                      // Prepare parsing
                      zypp::Pathname descr_dir = _repoindex->descrdir; // path below reporoot
                      //_datadir  = _repoIndex->datadir;  // path below reporoot

                      std::map<std::string,zypp::parser::susetags::RepoIndex::FileChecksumMap::const_iterator> availablePackageTranslations;

                      for_( it, _repoindex->metaFileChecksums.begin(), _repoindex->metaFileChecksums.end() )
                      {
                        // omit unwanted translations
                        if ( zypp::str::hasPrefix( it->first, "packages" ) )
                        {
                          static const zypp::str::regex rx_packages( "^packages((.gz)?|(.([^.]*))(.gz)?)$" );
                          zypp::str::smatch what;

                          if ( zypp::str::regex_match( it->first, what, rx_packages ) ) {
                            if ( what[4].empty() // packages(.gz)?
                              || what[4] == "DU"
                              || what[4] == "en" )
                            { ; /* always downloaded */ }
                            else if ( what[4] == "FL" )
                            { continue; /* never downloaded */ }
                            else
                            {
                              // remember and decide later
                              availablePackageTranslations[what[4]] = it;
                              continue;
                            }
                          }
                          else
                            continue; // discard

                        } else if ( it->first == "patterns.pat"
                                  || it->first == "patterns.pat.gz" ) {
                          // take all patterns in one go

                        }  else if ( zypp::str::endsWith( it->first, ".pat" )
                                  || zypp::str::endsWith( it->first, ".pat.gz" ) ) {

                          // *** see also zypp/parser/susetags/RepoParser.cc ***

                          // omit unwanted patterns, see https://bugzilla.novell.com/show_bug.cgi?id=298716
                          // expect "<name>.<arch>.pat[.gz]", <name> might contain additional dots
                          // split at dots, take .pat or .pat.gz into account

                          std::vector<std::string> patparts;
                          unsigned archpos = 2;
                          // expect "<name>.<arch>.pat[.gz]", <name> might contain additional dots
                          unsigned count = zypp::str::split( it->first, std::back_inserter(patparts), "." );
                          if ( patparts[count-1] == "gz" )
                              archpos++;

                          if ( count > archpos ) {
                            try { // might by an invalid architecture
                              zypp::Arch patarch( patparts[count-archpos] );
                              if ( !patarch.compatibleWith( zConfig().systemArchitecture() ) ) {
                                // discard, if not compatible
                                MIL << "Discarding pattern " << it->first << std::endl;
                                continue;
                              }

                            } catch ( const zypp::Exception & excpt )  {
                              WAR << "Pattern file name does not contain recognizable architecture: " << it->first << std::endl;
                              // keep .pat file if it doesn't contain an recognizable arch
                            }
                          }
                        }

                        MIL << "adding job " << it->first << std::endl;
                        auto location = zypp::OnMediaLocation( repoInfo().path() + descr_dir + it->first, 1 )
                            .setChecksum( it->second )
                            .setDeltafile( search_deltafile( _ctx->deltaDir() + descr_dir, it->first) );

                        requiredFiles.push_back( std::move(location) );
                      }


                      // check whether to download more package translations:
                      {
                        auto fnc_checkTransaltions( [&]( const zypp::Locale & locale_r ) {
                          for ( zypp::Locale toGet( locale_r ); toGet; toGet = toGet.fallback() ) {
                            auto it( availablePackageTranslations.find( toGet.code() ) );
                            if ( it != availablePackageTranslations.end() ) {
                              auto mit( it->second );
                              MIL << "adding job " << mit->first << std::endl;
                              requiredFiles.push_back( zypp::OnMediaLocation( repoInfo().path() + descr_dir + mit->first, 1 )
                                                           .setChecksum( mit->second )
                                                           .setDeltafile( search_deltafile( deltaDir() + descr_dir, mit->first) ));
                              break;
                            }
                          }
                        });

                        for ( const zypp::Locale & it : zConfig().repoRefreshLocales() ) {
                          fnc_checkTransaltions( it );
                        }
                        fnc_checkTransaltions( zConfig().textLocale() );
                      }

                      for( const auto &it : _repoindex->mediaFileChecksums ) {
                        // Repo adopts license files listed in HASH
                        if ( it.first != "license.tar.gz" )
                          continue;

                        MIL << "adding job " << it.first << std::endl;
                        requiredFiles.push_back( zypp::OnMediaLocation ( repoInfo().path() + it.first, 1 )
                                                     .setChecksum( it.second )
                                                     .setDeltafile( search_deltafile( deltaDir(), it.first ) ));
                      }

                      for( const auto &it : _repoindex->signingKeys ) {
                        MIL << "adding job " << it.first << std::endl;
                        zypp::OnMediaLocation location( repoInfo().path() + it.first, 1 );
                        location.setChecksum( it.second );
                        requiredFiles.push_back( std::move(location) );
                      }

                    } catch ( const zypp::Exception &e ) {
                      ZYPP_CAUGHT( e );
                      return makeReadyResult(expected<DlContextRefType>::error( std::make_exception_ptr(e) ) );
                    } catch ( ... ) {
                      return makeReadyResult(expected<DlContextRefType>::error( std::current_exception() ) );
                    }

                    // add the required files to the base steps
                    if ( _progressObserver ) _progressObserver->setBaseSteps ( _progressObserver->baseSteps () + requiredFiles.size() );

                    return transform_collect  ( std::move(requiredFiles), [this]( zypp::OnMediaLocation file ) {

                      return DownloadWorkflow::provideToCacheDir( _ctx, _mediaHandle, file.filename(), ProvideFileSpec(file) )
                          | inspect ( incProgress( _progressObserver ) );

                    }) | and_then ( [this]( std::vector<zypp::ManagedFile> &&dlFiles ) {
                      auto &downloadedFiles = _ctx->files();
                      downloadedFiles.insert( downloadedFiles.end(), std::make_move_iterator(dlFiles.begin()), std::make_move_iterator(dlFiles.end()) );
                      return expected<DlContextRefType>::success( std::move(_ctx) );
                    });
                  });

          } | finishProgress( _progressObserver );
      }

    private:

      const zypp::RepoInfo &repoInfo() const {
        return _ctx->repoInfo();
      }

      const zypp::filesystem::Pathname &deltaDir() const {
        return _ctx->deltaDir();
      }

      zypp::ZConfig &zConfig() {
        return _ctx->zyppContext()->config();
      }

      DlContextRefType _ctx;
      zypp::parser::susetags::RepoIndex_Ptr _repoindex;
      MediaHandle _mediaHandle;
      ProgressObserverRef _progressObserver;
    };
  }

  AsyncOpRef<expected<repo::AsyncDownloadContextRef> > download(repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver)
  {
    return SimpleExecutor< DlLogic, AsyncOp<expected<repo::AsyncDownloadContextRef>> >::run( std::move(dl), std::move(mediaHandle), std::move(progressObserver) );
  }

  expected<repo::SyncDownloadContextRef> download(repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle, ProgressObserverRef progressObserver)
  {
    return SimpleExecutor< DlLogic, SyncOp<expected<repo::SyncDownloadContextRef>> >::run( std::move(dl), std::move(mediaHandle), std::move(progressObserver) );
  }

}
