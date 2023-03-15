/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPPNG_CONTEXT_H
#define ZYPPNG_CONTEXT_H

/*!
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
 * context = Zypp.Context()
 * print(context.version())
 * \endcode
 *
 *
 */


#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ZyppRepoManager ZyppRepoManager;
typedef struct _ZyppDownloader ZyppDownloader;

/**
 * ZyppContext: glibContext: (transfer full)
 *
 * The glib main context zypper should use to execute async code, if not given zypp
 * will create its own context.
 */

#define ZYPP_TYPE_CONTEXT (zypp_context_get_type ())
G_DECLARE_FINAL_TYPE ( ZyppContext, zypp_context, ZYPP, CONTEXT, GObject )

/**
 * zypp_context_load_system:
 * @sysRoot: (nullable): The system sysroot to load, if a nullptr is given "/" is used
 *
 * Loads the system at the given sysroot, returns TRUE on success, otherwise FALSE
 */
gboolean zypp_context_load_system ( ZyppContext *self, const gchar *sysRoot );

const gchar * zypp_context_version ( ZyppContext *self );
void zypp_context_set_version ( ZyppContext *self, const gchar * ver );

/**
 * zypp_context_get_repo_manager:
 *
 * Returns: (transfer none): the zyppng repomanager
 */
ZyppRepoManager *zypp_context_get_repo_manager ( ZyppContext *self );

/**
 * zypp_context_get_downloader:
 *
 * Returns: (transfer none): the #ZyppDownloader instance belonging to this context
 */
ZyppDownloader *zypp_context_get_downloader ( ZyppContext *self );


G_END_DECLS

#ifdef  __cplusplus

#include <zyppng/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppContext, zypp_context, ZYPP, CONTEXT )
#endif

#endif // ZYPPNG_CONTEXT_H
