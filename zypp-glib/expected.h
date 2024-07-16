/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_EXPECTED_H
#define ZYPP_GLIB_EXPECTED_H

#include <glib.h>
#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

typedef struct _ZyppExpected ZyppExpected;

#define ZYPP_TYPE_EXPECTED (zypp_expected_get_type ())

#pragma GCC visibility push(default)
G_DECLARE_FINAL_TYPE ( ZyppExpected, zypp_expected, ZYPP, EXPECTED, GObject )
#pragma GCC visibility pop

/**
 * zypp_expected_new_value: (constructor)
 *
 * Creates a new ZyppExcpected containing a value
 */
ZyppExpected *zypp_expected_new_value( const GValue *value ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_expected_new_error: (constructor)
 *
 * Creates a new ZyppExcpected containing a error
 */
ZyppExpected *zypp_expected_new_error(GError *error) LIBZYPP_GLIB_EXPORT;

gboolean zypp_expected_has_value(ZyppExpected *self) LIBZYPP_GLIB_EXPORT;
gboolean zypp_expected_has_error(ZyppExpected *self) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_expected_get_error:
 *
 * Returns: (transfer none) (nullable): The error or NULL if there is no error
 */
const GError *zypp_expected_get_error(ZyppExpected *self) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_expected_get_value:
 *
 * Returns: (transfer none) (nullable): The value or NULL if there is none or a error
 */
const GValue *zypp_expected_get_value( ZyppExpected *self, GError **error ) LIBZYPP_GLIB_EXPORT;

G_END_DECLS

#ifdef  __cplusplus

#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppExpected, zypp_expected, ZYPP, EXPECTED )
#endif

#endif
