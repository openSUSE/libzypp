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
 */


#include <glib-object.h>

G_BEGIN_DECLS

#define ZYPP_TYPE_CONTEXT (zypp_context_get_type ())
G_DECLARE_FINAL_TYPE ( ZyppContext, zypp_context, ZYPP, CONTEXT, GObject )

G_END_DECLS

#endif // ZYPPNG_CONTEXT_H
