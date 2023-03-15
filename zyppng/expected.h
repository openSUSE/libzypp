/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPPNG_EXPECTED_H
#define ZYPPNG_EXPECTED_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ZyppExpected ZyppExpected;

#define ZYPP_TYPE_EXPECTED (zypp_expected_get_type ())
G_DECLARE_FINAL_TYPE ( ZyppExpected, zypp_expected, ZYPP, EXPECTED, GObject )

ZyppExpected *zypp_expected_new_value( const GValue *value );
ZyppExpected *zypp_expected_new_error(GError *error);

gboolean zypp_expected_has_value(ZyppExpected *self);
gboolean zypp_expected_has_error(ZyppExpected *self);

/**
 * zypp_expected_get_error:
 *
 * Returns: (transfer none) (nullable): The error or NULL if there is no error
 */
const GError *zypp_expected_get_error(ZyppExpected *self);

/**
 * zypp_expected_get_value:
 *
 * Returns: (transfer none) (nullable): The value or NULL if there is none or a error
 */
const GValue *zypp_expected_get_value( ZyppExpected *self, GError **error );

G_END_DECLS

#ifdef  __cplusplus

#include <zyppng/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppExpected, zypp_expected, ZYPP, EXPECTED )
#endif

#endif
