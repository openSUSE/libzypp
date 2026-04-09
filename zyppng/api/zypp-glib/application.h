/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_APPLICATION_H
#define ZYPP_GLIB_APPLICATION_H

/*
 * The zyppng API library aims to provide a new set of APIs for the libzypp API which
 * makes it possible to use libzypp from different programming languages and offers a
 * more high level and stable API than the original library.
 *
 * In order to support using multiple languages, zyppng will leverage GObject and GIR technologies,
 * featuring a pure glib based C API as described here: https://gi.readthedocs.io/en/latest/index.html
 *
 * The ultimate goal of this project is to function as the only officially supported API for compiling against
 * zypp. Tools like zypper will be rewritten to use this API set.
 *
 * \code {.python}
 * #!/usr/bin/env python3
 *
 * import gi.repository
 *
 * # Set the search path to use the newly generated introspection files, only required if they are not in the default directories
 * gi.require_version('GIRepository', '2.0')
 * from gi.repository import GIRepository
 * GIRepository.Repository.prepend_search_path('/home/zbenjamin/build/libzypp/zyppng')
 *
 * gi.require_version('Zypp', '1.0')
 * from gi.repository import Zypp
 *
 * zyppApplication = Zypp.Application()
 * context = Zypp.Context()
 * print(context.version())
 * \endcode
 */

#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

#define ZYPP_TYPE_APPLICATION (zypp_application_get_type ())

#pragma GCC visibility push(default)
G_DECLARE_DERIVABLE_TYPE ( ZyppApplication, zypp_application, ZYPP, APPLICATION, GObject )
#pragma GCC visibility pop

struct LIBZYPP_GLIB_EXPORT _ZyppApplicationClass {
  GObjectClass parent_class;
  gpointer padding[12];
};

/**
 * zypp_application_new: (constructor):
 * @eventDispatcher: (transfer none) (nullable): The event dispatcher to use when running async code.
 *
 * Creates a new ZyppApplication instance, if there is already a existing instance this one is returned instead.
 * If there is no event dispatcher passed to the constructor, one is created automatically.
 *
 * Returns: (transfer full): Reference to the new Application object
 */
ZyppApplication *zypp_application_new( GMainContext *eventDispatcher ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_application_get_dispatcher:
 *
 * Returns the internally used event dispatcher, this is at least valid as long as the Application
 * instance is valid.
 *
 * Returns: (nullable) (transfer none): The event dispatcher used by zypp
 */
GMainContext *zypp_application_get_dispatcher( ZyppApplication *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_application_get_version:
 * Returns: The libzypp version string
 */
const gchar * zypp_application_get_version ( ZyppApplication *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_application_set_user_data:
 * @userData: (transfer none) (nullable): The userdata string
 *
 * Set's a user defined string to be passed to log, history, plugins.
 * Passing a nullptr string clears the userdata
 */
void zypp_application_set_user_data ( ZyppApplication *self, const gchar *userData ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_application_has_user_data:
 *
 * Returns: True if a userdata string has been set
 */
gboolean zypp_application_has_user_data ( ZyppApplication *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_application_get_user_data:
 *
 * Returns: (transfer none) (nullable): The currently used userdata string, or NULL if none was set
 */
const gchar *zypp_application_get_user_data ( ZyppApplication *self ) LIBZYPP_GLIB_EXPORT;

G_END_DECLS


#endif
