/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_LIST_CHOICE_MESSAGE_REQUEST_H
#define ZYPP_GLIB_LIST_CHOICE_MESSAGE_REQUEST_H

#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>
#include <zypp-glib/ui/userrequest.h>

G_BEGIN_DECLS


#define ZYPP_TYPE_LIST_CHOICE_OPTION (zypp_list_choice_option_get_type())

/**
 * ZyppListChoiceOption:
 *
 * One possible option for the user to select from, a option
 * always consists of a label and a detail. The label is the short
 * string that can be shown on a button or in a drop box , the detail is the description.
 *
 * For example:
 * {"y", "Info about y"}, {"n", "Info about n"}, {"d", "Info about d"}
 *
 */
typedef struct _ZyppListChoiceOption ZyppListChoiceOption;

/**
 * zypp_list_choice_option_copy: (skip):
 *
 * Copy the boxed type
 */
ZyppListChoiceOption * zypp_list_choice_option_copy ( ZyppListChoiceOption *r ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_list_choice_option_free: (skip):
 *
 * Free the boxed type
 */
void zypp_list_choice_option_free ( ZyppListChoiceOption *r ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_list_choice_option_get_label:
 *
 * Get the short label string for this option
 */
const char * zypp_list_choice_option_get_label( ZyppListChoiceOption *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_list_choice_option_get_detail:
 *
 * Get the detail string for this option
 */
const char * zypp_list_choice_option_get_detail( ZyppListChoiceOption *self ) LIBZYPP_GLIB_EXPORT;


/**
 * ZyppListChoiceRequest:
 *
 * The `ZyppListChoiceRequest` event represents a choice request to the user,
 * the possible answers are given via a GList of possible choices.
 *
 * |[<!-- language="C" -->
 * GList *choices = zypp_list_choice_request_get_options(msg);
 * for (GList *l = choices; l != NULL; l = l->next)
 * {
 *   ZyppListChoiceOption *option = l->data;
 *   printf("Option: %s with detail: %s")
 *    , zypp_list_choice_option_get_label(option)
 *    , zypp_list_choice_option_get_detail(option));
 * }
 * g_list_free_full( choices, zypp_list_choice_option_free );
 * ]|
 *
 */

#pragma GCC visibility push(default)
G_DECLARE_FINAL_TYPE ( ZyppListChoiceRequest, zypp_list_choice_request, ZYPP, LIST_CHOICE_REQUEST, GObject )
#pragma GCC visibility pop

#define ZYPP_TYPE_LIST_CHOICE_REQUEST (zypp_list_choice_request_get_type ())

const char * zypp_list_choice_request_get_label( ZyppListChoiceRequest *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_list_choice_request_get_options:
 *
 * Returns: (element-type ZyppListChoiceOption) (transfer full): list of the available answer options
 */
GList  *zypp_list_choice_request_get_options ( ZyppListChoiceRequest *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_list_choice_request_set_choice:
 *
 * Remember the users choice, this will be returned to the code that triggered the event.
 */
void zypp_list_choice_request_set_choice ( ZyppListChoiceRequest *self, guint choice ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_list_choice_request_get_choice:
 *
 * Get the current selected choice
 */
guint zypp_list_choice_request_get_choice( ZyppListChoiceRequest *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_list_choice_request_get_default:
 *
 * Get the default choice, this will be selected if is not overriden by the receiving code
 */
guint zypp_list_choice_request_get_default ( ZyppListChoiceRequest *self ) LIBZYPP_GLIB_EXPORT;

G_END_DECLS

#ifdef  __cplusplus
#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppListChoiceRequest, zypp_list_choice_request, ZYPP, LIST_CHOICE_REQUEST )
ZYPP_GLIB_DEFINE_GLIB_SIMPLE_TYPE( ZyppListChoiceOption, zypp_list_choice_option_free, ZYPP_GLIB_ADD_GLIB_COPYTRAIT(zypp_list_choice_option_copy) )
#endif

#endif // ZYPP_GLIB_LIST_CHOICE_MESSAGE_REQUEST_H
