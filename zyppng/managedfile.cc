/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "private/managedfile_p.h"
#include <zypp-core/ManagedFile.h>
#include <zypp-core/fs/PathInfo.h>
#include <iostream>

ZyppManagedFile *zypp_managed_file_new( const gchar *path, gboolean dispose )
{
  std::unique_ptr<ZyppManagedFile> ptr( new ZyppManagedFile() );
  ptr->_file = zypp::ManagedFile( zypp::Pathname(path) );
  if ( dispose )
    ptr->_file.setDispose( zypp::filesystem::unlink );

  return ptr.release();
}

ZyppManagedFile * zypp_managed_file_copy ( ZyppManagedFile *r )
{
  return zypp_managed_file_new( r->_file );
}

void zypp_managed_file_free ( ZyppManagedFile *r )
{
  delete r;
}

G_DEFINE_BOXED_TYPE (ZyppManagedFile, zypp_managed_file,
                     zypp_managed_file_copy,
                     zypp_managed_file_free)


ZyppManagedFile *zypp_managed_file_new( const zypp::ManagedFile &f )
{
  return (new _ZyppManagedFile{f});
}

void zypp_managed_file_set_dispose_enabled( ZyppManagedFile *self, gboolean enabled )
{
  if ( enabled )
    self->_file.setDispose( zypp::filesystem::unlink );
  else
    self->_file.resetDispose();
}

gchar *zypp_managed_file_get_path( ZyppManagedFile *self )
{
  return g_strdup( self->_file->c_str() );
}
