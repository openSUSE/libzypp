/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/source/RepoProvideFile.cc
 *
*/
#include <zypp-core/ng/pipelines/transform.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>

#include <zypp/base/Gettext.h>
#include <zypp/base/Logger.h>
#include <zypp/base/String.h>
#include <utility>
#include <zypp-core/base/UserRequestException>
#include <zypp/repo/RepoProvideFile.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/MediaSetAccess.h>
#include <zypp/ZConfig.h>
#include <zypp/ZYppFactory.h>
#include <zypp/repo/SUSEMediaVerifier.h>
#include <zypp/repo/RepoException.h>

#include <zypp/repo/SUSEMediaVerifier.h>
#include <zypp/repo/RepoException.h>
#include <zypp/FileChecker.h>
#include <zypp/Fetcher.h>

using std::endl;
using std::set;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace repo
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	provideFile
    //
    ///////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    namespace
    { /////////////////////////////////////////////////////////////////

      /** Hack to extract progress information from media::DownloadProgressReport.
       * We redirect the static report triggered from RepoInfo::provideFile
       * to feed the ProvideFilePolicy callbacks in addition to any connected
       * media::DownloadProgressReport.
      */
      struct DownloadFileReportHack : public callback::ReceiveReport<media::DownloadProgressReport>
      {
        using BaseType = callback::ReceiveReport<ReportType>;
        using RedirectType = function<bool (int)>;

        DownloadFileReportHack(RedirectType &&redirect_r)
          : _oldRec(Distributor::instance().getReceiver()),
            _redirect(std::move(redirect_r)) {
          connect();
        }

        DownloadFileReportHack(const DownloadFileReportHack &) = delete;
        DownloadFileReportHack(DownloadFileReportHack &&) = delete;
        DownloadFileReportHack &operator=(const DownloadFileReportHack &) = delete;
        DownloadFileReportHack &operator=(DownloadFileReportHack &&) = delete;

        ~DownloadFileReportHack() override
        { if ( _oldRec ) Distributor::instance().setReceiver( *_oldRec ); else Distributor::instance().noReceiver(); }

        void start( const Url & file, Pathname localfile ) override
        {
          if ( _oldRec )
            _oldRec->start( file, localfile );
          else
            BaseType::start( file, localfile );
        }

        bool progress( int value, const Url & file, double dbps_avg = -1, double dbps_current = -1 ) override
        {
          bool ret = true;
          if ( _oldRec )
            ret &= _oldRec->progress( value, file, dbps_avg, dbps_current );
          if ( _redirect )
            ret &= _redirect( value );
          return ret;
        }

        Action problem( const Url & file, Error error, const std::string & description ) override
        {
          if ( _oldRec )
            return _oldRec->problem( file, error, description );
          return BaseType::problem( file, error, description );
        }
        void finish( const Url & file, Error error, const std::string & reason ) override
        {
          if ( _oldRec )
            _oldRec->finish( file, error, reason );
          else
            BaseType::finish( file, error, reason );
        }

        private:
          Receiver * _oldRec;
          RedirectType _redirect;
      };

      /////////////////////////////////////////////////////////////////
    } // namespace
    ///////////////////////////////////////////////////////////////////

    ManagedFile provideFile( RepoInfo repo_r,
                             const OnMediaLocation & loc_r,
                             const ProvideFilePolicy & policy_r )
    {
      RepoMediaAccess access;
      return access.provideFile(std::move(repo_r), loc_r, policy_r );
    }

    ///////////////////////////////////////////////////////////////////
    class RepoMediaAccess::Impl
    {
    public:
      Impl(ProvideFilePolicy &&defaultPolicy_r)
        : _defaultPolicy(std::move(defaultPolicy_r)) {}

      Impl(const Impl &) = delete;
      Impl(Impl &&) = delete;
      Impl &operator=(const Impl &) = delete;
      Impl &operator=(Impl &&) = delete;

      ~Impl()
      {
        std::map<Url, shared_ptr<MediaSetAccess> >::iterator it;
        for ( it = _medias.begin();
              it != _medias.end();
              ++it )
        {
          it->second->release();
        }
      }

      /** Provide a MediaSetAccess for \c url with label and verifier adjusted.
       *
       * As the same url (e.g. \c 'dvd:///' ) might be used for multiple repos
       * we must always adjust the repo specific data (label,verifier).
       *
       * \todo This mixture of media and repos specific data is fragile.
      */
      shared_ptr<MediaSetAccess> mediaAccessForUrl( const MirroredOrigin &origin, RepoInfo repo )
      {
        if ( !origin.isValid() )
          return nullptr;

        std::map<Url, shared_ptr<MediaSetAccess> >::const_iterator it;
        it = _medias.find( origin.authority().url() ); // primary Url is the key
        shared_ptr<MediaSetAccess> media;
        if ( it != _medias.end() )
        {
          media = it->second;
        }
        else
        {
          media.reset( new MediaSetAccess( origin ) );
          _medias[origin.authority().url()] = media;
        }
        setVerifierForRepo( repo, media );
        return media;
      }

      private:
        void setVerifierForRepo( const RepoInfo& repo, const shared_ptr<MediaSetAccess>& media )
        {
          // Always set the MediaSetAccess label.
          media->setLabel( repo.name() );

          // set a verifier if the repository has it

          Pathname mediafile = repo.metadataPath() + "/media.1/media";
          if ( ! repo.metadataPath().empty() )
          {
            if ( PathInfo(mediafile).isExist() )
            {
              std::map<shared_ptr<MediaSetAccess>, RepoInfo>::const_iterator it;
              it = _verifier.find(media);
              if ( it != _verifier.end() )
              {
                if ( it->second.alias() == repo.alias() )
                {
                  // this media is already using this repo verifier
                  return;
                }
              }

              SUSEMediaVerifier lverifier { mediafile };
              if ( lverifier ) {
                DBG << "Verifier for repo '" << repo.alias() << "':" << lverifier << endl;
                for ( media::MediaNr i = 1; i <= lverifier.totalMedia(); ++i ) {
                  media::MediaVerifierRef verifier( new repo::SUSEMediaVerifier( lverifier, i ) );
                  media->setVerifier( i, verifier);
                }
                _verifier[media] = repo;
              }
              else {
                WAR << "Invalid verifier for repo '" << repo.alias() << "' in '" << repo.metadataPath() << "': " << lverifier << endl;
              }
            }
            else
            {
              DBG << "No media verifier for repo '" << repo.alias() << "' media.1/media does not exist in '" << repo.metadataPath() << "'" << endl;
            }
          }
          else
          {
            WAR << "'" << repo.alias() << "' metadata path is empty. Can't set verifier. Probably this repository does not come from RepoManager." << endl;
          }
        }

      private:
        std::map<shared_ptr<MediaSetAccess>, RepoInfo> _verifier;
        std::map<Url, shared_ptr<MediaSetAccess> > _medias;

      public:
        ProvideFilePolicy _defaultPolicy;
    };
    ///////////////////////////////////////////////////////////////////


    RepoMediaAccess::RepoMediaAccess( ProvideFilePolicy defaultPolicy_r )
      : _impl( new Impl( std::move(defaultPolicy_r) ) )
    {}

    RepoMediaAccess::~RepoMediaAccess()
    {}

    void RepoMediaAccess::setDefaultPolicy( const ProvideFilePolicy & policy_r )
    { _impl->_defaultPolicy = policy_r; }

    const ProvideFilePolicy & RepoMediaAccess::defaultPolicy() const
    { return _impl->_defaultPolicy; }

    ManagedFile RepoMediaAccess::provideFile( const RepoInfo& repo_r,
                                              const OnMediaLocation & loc_rx,
                                              const ProvideFilePolicy & policy_r )
    {
      const OnMediaLocation locWithPath( OnMediaLocation(loc_rx).prependPath( repo_r.path() ) );

      MIL << locWithPath << endl;
      // Arrange DownloadFileReportHack to recieve the source::DownloadFileReport
      // and redirect download progress triggers to call the ProvideFilePolicy
      // callback.
      DownloadFileReportHack dumb( std::bind( std::mem_fn(&ProvideFilePolicy::progress), std::ref(policy_r), _1 ) );

      RepoException repo_excpt(repo_r,
                               str::form(_("Can't provide file '%s' from repository '%s'"),
                               locWithPath.filename().c_str(),
                               repo_r.alias().c_str() ) );

      const auto &repoOrigins = repo_r.repoOrigins();

      if ( repoOrigins.empty() ) {
        repo_excpt.remember(RepoException(_("No url in repository.")));
        ZYPP_THROW(repo_excpt);
      }

      Fetcher fetcher;
      fetcher.addCachePath( repo_r.packagesPath() );
      MIL << "Added cache path " << repo_r.packagesPath() << endl;
      fetcher.addCachePath( repo_r.predownloadPath(), Fetcher::CleanFiles );
      MIL << "Added cache path " << repo_r.predownloadPath() << endl;

      // Test whether download destination is writable, if not
      // switch into the tmpspace (e.g. bnc#755239, download and
      // install srpms as user).
      Pathname destinationDir( repo_r.packagesPath() );

      PathInfo pi( destinationDir );
      if ( ! pi.isExist() )
      {
        // try to create it...
        assert_dir( destinationDir );
        pi();
      }
      if ( geteuid() != 0 && ! pi.userMayW() )
      {
        WAR << "Destination dir '" << destinationDir << "' is not user writable, using tmp space." << endl;
        destinationDir = getZYpp()->tmpPath() / destinationDir;
        assert_dir( destinationDir );
        fetcher.addCachePath( destinationDir );
        MIL << "Added cache path " << destinationDir << endl;
      }

      // Suppress (interactive) media::MediaChangeReport if we have fallback URLs
      media::ScopedDisableMediaChangeReport guard( repoOrigins.hasFallbackUrls() );
      for ( const auto &origin : repoOrigins )
      {

        if ( !origin.isValid() ) {
          MIL << "Skipping empty Url group" << std::endl;
          continue;
        }

        try
        {
          MIL << "Providing file of repo '" << repo_r.alias() << "' from: ";
          std::for_each( origin.begin (), origin.end(), [&]( const OriginEndpoint &u ){
            MIL << u << ", ";
          });
          MIL << std::endl;

          shared_ptr<MediaSetAccess> access = _impl->mediaAccessForUrl( origin, repo_r );
          if ( !access )
            continue;

          fetcher.enqueue( locWithPath, policy_r.fileChecker() );
          fetcher.start( destinationDir, *access );

          // reached if no exception has been thrown, so this is the correct file
          // LEGACY NOTE:
          // DeltaRpms e.g are in fact optional resources (tagged in OnMediaLocation).
          // They should actually return a ManagedFile(). But I don't know if there is
          // already code which requests optional files but relies on provideFile returning
          // a ManagedFile holding the path even if it does not exist. So I keep it this way.
          ManagedFile ret( destinationDir + locWithPath.filename() );
          if ( !repo_r.keepPackages() )
          {
            ret.setDispose( filesystem::unlink );
          }

          MIL << "provideFile at " << ret << endl;
          return ret;
        }
        catch ( const UserRequestException & excpt )
        {
          ZYPP_RETHROW( excpt );
        }
        catch ( const FileCheckException & excpt )
        {
          ZYPP_RETHROW( excpt );
        }
        catch ( const Exception &e )
        {
          ZYPP_CAUGHT( e );

          repo_excpt.remember(e);

          WAR << "Trying next url" << endl;
          continue;
        }
      } // iteration over urls

      ZYPP_THROW(repo_excpt);
      return ManagedFile(); // not reached
    }

    /////////////////////////////////////////////////////////////////
  } // namespace repo
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
