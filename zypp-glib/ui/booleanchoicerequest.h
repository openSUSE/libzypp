/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_BOOLEAN_CHOICE_MESSAGE_REQUEST_H
#define ZYPP_GLIB_BOOLEAN_CHOICE_MESSAGE_REQUEST_H

#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>
#include <zypp-glib/ui/userrequest.h>

G_BEGIN_DECLS


/**
 * ZyppBooleanChoiceRequest:
 *
 * The `ZyppBooleanChoiceRequest` event represents a choice request to the user,
 * the possible answers are true or false.
 *
 */

#pragma GCC visibility push(default)
G_DECLARE_FINAL_TYPE ( ZyppBooleanChoiceRequest, zypp_boolean_choice_request, ZYPP, BOOLEAN_CHOICE_REQUEST, GObject )
#pragma GCC visibility pop

#define ZYPP_TYPE_BOOLEAN_CHOICE_REQUEST (zypp_boolean_choice_request_get_type ())

const char * zypp_boolean_choice_request_get_label( ZyppBooleanChoiceRequest *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_boolean_choice_request_set_choice:
 *
 * Remember the users choice, this will be returned to the code that triggered the event.
 */
void zypp_boolean_choice_request_set_choice ( ZyppBooleanChoiceRequest *self, gboolean choice ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_boolean_choice_request_get_choice:
 *
 * Get the current choice
 */
gboolean zypp_boolean_choice_request_get_choice( ZyppBooleanChoiceRequest *self ) LIBZYPP_GLIB_EXPORT;


G_END_DECLS

#ifdef  __cplusplus
#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppBooleanChoiceRequest, zypp_boolean_choice_request, ZYPP, BOOLEAN_CHOICE_REQUEST )
#endif

#endif // ZYPP_GLIB_BOOLEAN_CHOICE_MESSAGE_REQUEST_H
