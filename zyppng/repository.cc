/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/repository_p.h"
#include "private/repoinfo_p.h"
#include <zypp/Repository.h>
#include <iostream>

struct _ZyppRepository
{
  GObjectClass            parent_class;
  struct Cpp {
    zypp::Repository _repo;
    ZyppRepoManager *_repoManager;
  } _data;
};
G_DEFINE_TYPE(ZyppRepository, zypp_repository, G_TYPE_OBJECT)

static void zypp_repository_dispose (GObject *gobject)
{
  /* In dispose(), you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a
   * reference.
   */

  // always call parent class dispose
  G_OBJECT_CLASS (zypp_repository_parent_class)->dispose (gobject);
}

static void zypp_repository_finalize (GObject *gobject)
{
  ZyppRepository *ptr = ZYPP_REPOSITORY(gobject);
  g_clear_weak_pointer( &ptr->_data._repoManager );
  ptr->_data.~Cpp();

  // always call parent class finalizer
  G_OBJECT_CLASS (zypp_repository_parent_class)->finalize (gobject);
}

void zypp_repository_class_init( ZyppRepositoryClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = zypp_repository_dispose;
  object_class->finalize = zypp_repository_finalize;
}

void zypp_repository_init( ZyppRepository *ctx )
{
  new ( &ctx->_data ) ZyppRepository::Cpp();
}

ZyppRepository *zypp_repository_new( const zypp::Repository &cppVersion, ZyppRepoManager *mgr )
{
  ZyppRepository *obj = (ZyppRepository *)g_object_new( zypp_repository_get_type(), NULL );
  obj->_data._repo = cppVersion;
  g_set_weak_pointer( &obj->_data._repoManager, mgr );
  return obj;
}

gchar *zypp_repository_get_name( ZyppRepository *self )
{
  if ( !self || !self->_data._repoManager )
    return nullptr;
  return g_strdup( self->_data._repo.name().c_str() );
}

ZyppRepoInfo *zypp_repository_get_repoinfo(ZyppRepository *self)
{
  if ( !self || !self->_data._repoManager )
    return nullptr;
  return zypp_repo_info_wrap_cpp ( self->_data._repo.info () );
}
