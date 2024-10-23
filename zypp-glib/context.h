/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_CONTEXT_H
#define ZYPP_GLIB_CONTEXT_H

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


/**
 * ZyppContext:
 *
 * This class is the basic building block for the zypp glib API. It defines the path of the
 * root filesystem we are operating on. This is usually "/" but to support chroot use cases it
 * can point to any directory in a filesystem where packages should be installed into. If the rootfs
 * is not defined as "/" then zypp will install packages using chroot into the directory.
 *
 * Settings for zypp are loaded from the rootfs directory and locks are also applied relative to it.
 * Meaning that one context can operate on "/" while another one can operate on "/tmp/rootfs".
 */


#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>
G_BEGIN_DECLS

/**
 * ZyppContext: glibContext: (transfer full)
 *
 * The glib main context zypper should use to execute async code, if not given zypp
 * will create its own context.
 */


/**
 * ZyppContext: cppObj: (skip)
 */

#define ZYPP_TYPE_CONTEXT (zypp_context_get_type ())

#pragma GCC visibility push(default)
G_DECLARE_FINAL_TYPE ( ZyppContext, zypp_context, ZYPP, CONTEXT, GObject )
#pragma GCC visibility pop

/**
 * zypp_context_load_system:
 * @sysRoot: (nullable): The system sysroot to load, if a nullptr is given "/" is used
 *
 * Loads the system at the given sysroot, returns TRUE on success, otherwise FALSE
 */
gboolean zypp_context_load_system ( ZyppContext *self, const gchar *sysRoot, GError **error ) LIBZYPP_GLIB_EXPORT;

const gchar * zypp_context_version ( ZyppContext *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_context_sysroot:
 *
 * Returns: (transfer full) (nullable): The context root as requested when loading the system
 */
gchar *zypp_context_sysroot( ZyppContext *self ) LIBZYPP_GLIB_EXPORT;

G_END_DECLS

#ifdef  __cplusplus

#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppContext, zypp_context, ZYPP, CONTEXT )
#endif

#endif // ZYPP_GLIB_CONTEXT_H
