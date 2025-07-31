#define INCLUDE_TESTSETUP_WITHOUT_BOOST 1
#include "../../tests/lib/TestSetup.h"


#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/base/String.h>
#include <zypp-core/zyppng/ui/ProgressObserver>
#include <zypp-core/zyppng/pipelines/Transform>
#include <zypp/ng/Context>
#include <zypp/ng/repo/downloader.h>
#include <zypp/RepoInfo.h>
#include <zypp/Url.h>
#include <zypp/ng/userrequest.h>
#include <zypp-media/ng/Provide>

#include <zypp/ng/repo/workflows/repodownloaderwf.h>
#include <zypp/ng/repo/workflows/repomanagerwf.h>
#include <zypp/ng/repomanager.h>

#include <zypp-tui/Application>
#include <zypp-tui/output/OutNormal.h>

#include <cstdlib>
#include <clocale>
#include <iostream>
#include <numeric>

#include <termios.h>
#include <fstream>
#include <unistd.h>
#include <sys/poll.h>
#include <readline/readline.h>

/*!
 *  Place terminal referred to by 'fd' in cbreak mode (noncanonical mode
 *  with echoing turned on).
 */

bool ttySetCbreak(int fd, struct termios *prevTermios)
{
    struct termios t;

    if (tcgetattr(fd, &t) == -1)
        return false;

    if (prevTermios != NULL)
        *prevTermios = t;

    // disable canonical mode
    t.c_lflag &= ~( ICANON );

    t.c_cc[VMIN] = 1;                   /* Character-at-a-time input */
    t.c_cc[VTIME] = 0;                  /* with blocking */

    if (tcsetattr(fd, TCSAFLUSH, &t) == -1)
        return false;

    return true;
}

bool setBlocking ( int fd, bool set = true ) {
  const int oldFlags = fcntl( fd, F_GETFL, 0 );
  if (oldFlags == -1) return false;

  const int flags = set ? ( oldFlags & ~(O_NONBLOCK) ) : ( oldFlags | O_NONBLOCK );
  if ( oldFlags == flags )
    return true;

  return ( fcntl( fd, F_SETFL, flags ) == 0 );
}




int main ( int argc, char *argv[] )
{
  using namespace zyppng::operators;

  ztui::Application appl;
  auto &out = appl.out ();

  try {
    auto ctx = zyppng::Context::create ();

    zypp::RepoInfo ri;
    ri.setName  ("Repo-Oss");
    ri.setAlias ("Repo-Oss");
    ri.setType ( zypp::repo::RepoType::RPMMD_e );
    ri.setBaseUrl( zypp::Url("https://download.opensuse.org/tumbleweed/repo/oss") );

    zypp::filesystem::assert_dir("/tmp/dltest");
    zypp::Pathname destdir("/tmp/dltest");

    ctx->sigEvent().connect([&]( zyppng::UserRequestRef req ){
      switch( req->type () ) {
        case zyppng::UserRequestType::Message:
          out.info ( static_cast<zyppng::ShowMessageRequest *>(req.get())->message () );
          break;
        case zyppng::UserRequestType::ListChoice: {
          auto r = static_cast<zyppng::ListChoiceRequest *>(req.get());
          ztui::PromptOptions popts;

          const auto &answers = r->answers ();
          {
            auto options = zyppng::transform ( answers, []( const auto &entry ) { return entry.opt; } );
            popts.setOptions( std::move(options), r->defaultAnswer() );
          }
          for( unsigned i = 0; i < answers.size(); i++ ) {
            popts.setOptionHelp ( i, answers.at(i).detail );
          }

          bool canContinue = true;
          std::ifstream stm( "/dev/tty" );
          while ( canContinue ) {
            appl.out().prompt( 0, r->label(), popts );
            std::string reply = str::getline( stm, str::TRIM );
            const auto &matches = popts.getReplyMatches ( reply );
            if ( matches.size () == 1 ) {
              //valid answer
              r->setChoice ( matches[0] );
              canContinue = false;
            } else if ( matches.size() > 1 ) {
              out.error ("Ambigous answer, try again");
            } else {
              out.error ("Invalid answer, try again");
            }
          }
          break;
        }
        case zyppng::UserRequestType::BooleanChoice: {
          auto r = static_cast<zyppng::BooleanChoiceRequest *>(req.get());
          std::string yn( std::string(_("yes")) + "/" + _("no") );
          ztui::PromptOptions popts( yn, 1 );
          appl.out().prompt( 0, r->label(), popts );

          std::ifstream stm( "/dev/tty" );
          std::string reply = str::getline( stm, str::TRIM );
          r->setChoice ( reply[0] == 'y' );
          break;
        }
        case zyppng::UserRequestType::KeyTrust: {
          auto r = static_cast<zyppng::TrustKeyRequest *>(req.get());

          std::string yn( std::string(_("(r)eject")) + "/" + _("trust-(a)lways") + "/" + _("(t)rust") );
          ztui::PromptOptions popts( yn, 0 );
          appl.out().prompt( 0, r->label(), popts );

          std::ifstream stm( "/dev/tty" );
          std::string reply = str::getline( stm, str::TRIM );
          r->setChoice ( reply[0] == 'a' ? zyppng::TrustKeyRequest::KEY_TRUST_AND_IMPORT : ( reply[0] == 't' ?  zyppng::TrustKeyRequest::KEY_TRUST_TEMPORARILY :  zyppng::TrustKeyRequest::KEY_DONT_TRUST ) );
          break;
        }
        case zyppng::UserRequestType::Custom:
          out.info ("Received request type: " + req->userData ().type ().asString () );
          break;
      }
    });


    auto observer = zyppng::ProgressObserver::create("Refreshing repositories", 2);
    observer->sigProgressChanged().connect([&](zyppng::ProgressObserver &o, double v){
      out.progress( "dl-repo", zypp::str::Str() << "Ref Progress changed to " << o.current() << "/" << o.steps (), o.progress () );
    });
    observer->sigFinished().connect([&](zyppng::ProgressObserver &, zyppng::ProgressObserver::FinishResult finRes ){
      out.info ( zypp::str::Str() << "Progress moved to finished with res: " << finRes << std::endl ); }
    );


    out.progressStart("dl-repo", "Refreshing repositories");

    std::vector<zypp::RepoInfo> allSysReps;
    {
      auto sysRepMgr = zyppng::AsyncRepoManager::create( ctx, zypp::RepoManagerOptions() ).unwrap();
      std::for_each (  sysRepMgr->repoBegin(), sysRepMgr->repoEnd(), [&]( zypp::RepoInfo info ){
        if ( info.enabled () ) {
          info.setMetadataPath (zypp::Pathname());
          info.setPackagesPath (zypp::Pathname());
          allSysReps.push_back(info);
        }
      });
    }

    auto repMgr = zyppng::AsyncRepoManager::create( ctx, zypp::RepoManagerOptions(destdir) ).unwrap();
    auto result = repMgr->refreshMetadata( std::move(allSysReps), zypp::RepoManagerFlags::RefreshIfNeeded, observer );
    observer->setFinished ();

    bool allSuccess = true;
    std::for_each( result.begin (), result.end(), [&]( const auto &resPair ) {
      if ( !resPair.second )
        allSuccess = false;

      try {
        resPair.second.unwrap();
        out.info ( zypp::str::Str() << "Refreshing repository: " << resPair.first.name() << " worked!" );
      } catch ( const zypp::Exception &e ) {
        out.error ( zypp::str::Str() << "Refreshing repository: " << resPair.first.name() << " failed with Error: " << e );
      } catch ( const std::exception &e ) {
        out.error ( zypp::str::Str() << "Refreshing repository: " << resPair.first.name() << " failed with Error: " <<  e.what() );
      } catch ( ... ) {
        out.error ( zypp::str::Str() << "Refreshing repository: " << resPair.first.name() << " failed with Error: Unknown exception" );
      }
    });

    out.progressEnd("dl-repo", "Refreshing repositories",  !allSuccess );

  } catch ( const zypp::Exception &e ) {
    out.error ( zypp::str::Str() << "Error: " << e << std::endl );
  } catch ( const std::exception &e ) {
    out.error ( zypp::str::Str() << "Error: " << e.what() << std::endl );
  } catch ( ... ) {
    out.error ( zypp::str::Str() << "Error: Unknown exception" << std::endl );
  }
  return 0;
}
