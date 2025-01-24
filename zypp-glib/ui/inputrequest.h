/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_INPUT_REQUEST_H
#define ZYPP_GLIB_INPUT_REQUEST_H

#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>
#include <zypp-glib/ui/userrequest.h>

G_BEGIN_DECLS


/**
 * ZyppInputRequest:
 *
 * The `ZyppInputRequest` event represents a generic input request to the user,
 * it can define a list of input fields that should be presented to the user and
 * can be accepted or rejected.
 *
 */

#pragma GCC visibility push(default)
G_DECLARE_FINAL_TYPE ( ZyppInputRequest, zypp_input_request, ZYPP, INPUT_REQUEST, GObject )
#pragma GCC visibility pop

#define ZYPP_TYPE_INPUT_REQUEST (zypp_input_request_get_type ())

/**
 * Type of a field in a ZyppInputRequest
 */
enum ZyppInputRequestFieldType {
  Text,
  Password
};

/**
 * ZyppInputRequestField:
 *
 * One possible option input field in a InputRequest. The type defines
 * which field should be rendered.
 *
 * For example:
 * {"y", "Info about y"}, {"n", "Info about n"}, {"d", "Info about d"}
 *
 */
typedef struct _ZyppInputRequestField ZyppInputRequestField;

/**
 * zypp_input_request_field_copy: (skip):
 *
 * Copy the boxed type
 */
ZyppInputRequestField * zypp_input_request_field_copy ( ZyppInputRequestField *r ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_input_request_field_free: (skip):
 *
 * Free the boxed type
 */
void zypp_input_request_field_free ( ZyppInputRequestField *r ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_input_request_field_get_type:
 *
 * Get the input type for this field
 */
ZyppInputRequestFieldType zypp_input_request_field_get_type( ZyppInputRequestField* field ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_input_request_field_get_label:
 *
 * Get the label string for this field
 */
const char *  zypp_input_request_field_get_label ( ZyppInputRequestField* field ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_input_request_field_get_value:
 *
 * Get the current value for the field
 */
const char *  zypp_input_request_field_get_value ( ZyppInputRequestField* field ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_input_request_get_label:
 *
 * Get the label string for this request
 */
const char *  zypp_input_request_get_label ( ZyppInputRequest* field ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_input_request_get_fields:
 *
 * Returns: (element-type ZyppInputRequestField) (transfer full): list of the available fields
 */
GList  *zypp_input_request_get_fields ( ZyppInputRequest *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_input_request_set_field_value:
 *
 * Sets the value of the field defined by fieldIndex.
 */
void zypp_input_request_set_field_value ( ZyppInputRequest *self, guint fieldIndex, const char* value );


G_END_DECLS

#ifdef  __cplusplus
#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppInputRequest, zypp_input_request, ZYPP, INPUT_REQUEST )
ZYPP_GLIB_DEFINE_GLIB_SIMPLE_TYPE( ZyppInputRequestField, zypp_input_request_field_free, ZYPP_GLIB_ADD_GLIB_COPYTRAIT(zypp_input_request_field_copy) )
#endif

#endif // ZYPP_GLIB_INPUT_REQUEST_H
