/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "private/repomanageroptions_p.h"
#include <zypp-core/base/String.h>
#include <memory>

ZyppRepoManagerOptions *zypp_repo_manager_options_new( const gchar *path )
{
  std::unique_ptr<ZyppRepoManagerOptions> ptr( new ZyppRepoManagerOptions() );
  ptr->_opts = zypp::RepoManagerOptions( zypp::str::asString( path ) );
  return ptr.release();
}

ZyppRepoManagerOptions * zypp_repo_manager_options_copy ( ZyppRepoManagerOptions *r )
{
  return zypp_repo_manager_options_new( r->_opts );
}

void zypp_repo_manager_options_free ( ZyppRepoManagerOptions *r )
{
  delete r;
}

G_DEFINE_BOXED_TYPE (ZyppRepoManagerOptions, zypp_repo_manager_options,
                     zypp_repo_manager_options_copy,
                     zypp_repo_manager_options_free)


ZyppRepoManagerOptions *zypp_repo_manager_options_new( const zypp::RepoManagerOptions &opts )
{
  return ( new _ZyppRepoManagerOptions{opts} );
}

gchar *zypp_repo_manager_options_get_root( ZyppRepoManagerOptions *self )
{
  return g_strdup( self->_opts.rootDir.c_str() );
}
