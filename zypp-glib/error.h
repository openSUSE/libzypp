/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_ERROR_H
#define ZYPP_GLIB_ERROR_H

#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

/**
 * ZYPP_ERROR:
 *
 * Error domain for the zypp exception handling. Errors in this domain will
 * be from the #ZyppException enumeration. See #GError for information
 * on error domains.
*/
#define ZYPP_EXCEPTION zypp_exception_quark ()

/**
 * ZyppError:
 * @ZYPP_ERROR: Generic Error that happend in the zypp API, check error string for details
 */
typedef enum
{
  ZYPP_ERROR
} ZyppException;

GQuark zypp_exception_quark () LIBZYPP_GLIB_EXPORT;



G_END_DECLS

#endif // ZYPP_GLIB_ERROR_H
