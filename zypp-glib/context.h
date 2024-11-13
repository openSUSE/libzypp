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

#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

typedef struct _ZyppProgressObserver ZyppProgressObserver;

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

/**
 * zypp_context_sysroot:
 *
 * Returns: (transfer full) (nullable): The context root as requested when loading the system
 */
gchar *zypp_context_sysroot( ZyppContext *self ) LIBZYPP_GLIB_EXPORT;


/**
 * zypp_context_get_progress_observer:
 *
 * Returns: (transfer full) (nullable): The currently used progress observer, or NULL if there was none registered.
 */
ZyppProgressObserver *zypp_context_get_progress_observer( ZyppContext *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_context_set_progress_observer:
 *
 * Sets the progress observer, this is where libzypp will generate progress and callback messages.
 * The context will hold a strong reference to the observer until it is resettet.
 */
void zypp_context_set_progress_observer( ZyppContext *self, ZyppProgressObserver *obs ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_context_reset_progress_observer:
 *
 * Clears the reference to the current progress observer, code will not be able to generate task information
 * for new tasks anymore, already running code however might still hold a reference to it.
 */
void zypp_context_reset_progress_observer( ZyppContext *self ) LIBZYPP_GLIB_EXPORT;



G_END_DECLS

#ifdef  __cplusplus

#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppContext, zypp_context, ZYPP, CONTEXT )
#endif

#endif // ZYPP_GLIB_CONTEXT_H
