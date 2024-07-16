
#include <zypp-glib/zypp-glib.h>
#include <zypp-glib/repoinfo.h>
#include <zypp-glib/utils/GList>
#include <iostream>

int main ( int argc, char *argv[] )
{
    zypp::glib::ZyppContextRef context = zypp::glib::zypp_context_create ( nullptr );
    if ( !context ) {
      std::cerr << "Could not create context, goodbye" << std::endl;
      return 1;
    }

    GError *err = nullptr;
    if ( !zypp_context_load_system ( context.get(), "/", &err ) ) {
      if ( err ) {
        std::cerr << "Loading the context failed: " << err->message << std::endl;
        return 1;
      } else {
        std::cerr << "Loading the context failed: unknown error!" << std::endl;
        return 1;
      }
    }

    zypp::glib::ZyppRepoManagerRef rMgr( zypp_repo_manager_new( context.get() ), zypp::glib::adopt_object );
    if ( !zypp_repo_manager_initialize( rMgr.get(), &err ) ) {
      if ( err ) {
        std::cerr << "Loading the context failed: " << err->message << std::endl;
        return 1;
      } else {
        std::cerr << "Loading the context failed: unknown error!" << std::endl;
        return 1;
      }
    }

    zypp::glib::GListView<ZyppRepoInfo> knownRepos( zypp_repo_manager_get_known_repos(rMgr.get()), zypp::glib::Ownership::Full );
    std::cout << "We know: " << knownRepos.size () << " repositories:" << std::endl;

    for ( int i = 0; i < knownRepos.size (); i++ ) {
      auto repo = *knownRepos[i];
      std::cout << "Repo : " << zypp_info_base_name( ZYPP_INFO_BASE( repo ) ) << std::endl;
    }

    return 0;

}
