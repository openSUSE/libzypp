/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_GLIB_MANAGEDFILE_H
#define ZYPP_GLIB_MANAGEDFILE_H

#include <glib.h>
#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

#define ZYPP_TYPE_MANAGED_FILE (zypp_managed_file_get_type())

typedef struct _ZyppManagedFile ZyppManagedFile;

GType zypp_managed_file_get_type (void) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_managed_file_new: (constructor):
 * @path: (in): The path of the file we want to manage
 * @dispose: If set to true the file is cleaned up when the #ZyppManagedFile gets out of scope
 */
ZyppManagedFile *zypp_managed_file_new( const gchar *path, gboolean dispose ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_managed_file_copy: (skip):
 *
 * Copy the boxed type
 */
ZyppManagedFile * zypp_managed_file_copy ( ZyppManagedFile *r ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_managed_file_free: (skip):
 *
 * Free the boxed type
 */
void zypp_managed_file_free ( ZyppManagedFile *r ) LIBZYPP_GLIB_EXPORT;


/**
 * zypp_managed_file_set_dispose_enabled:
 *
 * Enables or disables dispose for this object
 */
void zypp_managed_file_set_dispose_enabled( ZyppManagedFile *self, gboolean enabled ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_managed_file_get_path:
 *
 * Returns: (transfer full): The currently managed path
 */
gchar *zypp_managed_file_get_path( ZyppManagedFile *self ) LIBZYPP_GLIB_EXPORT;

G_END_DECLS

#endif // ZYPP_GLIB_MANAGEDFILE_H
