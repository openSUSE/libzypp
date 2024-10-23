/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_USERREQUEST_H
#define ZYPP_GLIB_USERREQUEST_H

#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS


#define ZYPP_TYPE_USER_REQUEST          (zypp_user_request_get_type ())

#pragma GCC visibility push(default)
G_DECLARE_INTERFACE (ZyppUserRequest, zypp_user_request, ZYPP, USER_REQUEST, GObject)
#pragma GCC visibility pop

/**
 * ZyppUserRequestType:
 * @ZYPP_USER_REQUEST_POPUP: As user request that has a label and predefined possible answers
 * @ZYPP_USER_REQUEST_MEDIACHANGE: Ask the user to change a medium
 *
 * Specifies the type of the event.
 */
typedef enum {
  ZYPP_USER_REQUEST_INVALID,
  ZYPP_USER_REQUEST_MESSAGE,
  ZYPP_USER_REQUEST_LIST_CHOICE,
  ZYPP_USER_REQUEST_BOOLEAN_CHOICE,
  ZYPP_USER_REQUEST_LAST        /* helper variable for decls */
} ZyppUserRequestType;


struct LIBZYPP_GLIB_EXPORT _ZyppUserRequestInterface
{
  GTypeInterface parent_iface;

  ZyppUserRequestType (*get_request_type)( ZyppUserRequest *self );
  GValue *(*get_data) ( ZyppUserRequest *self, const char *key );
  GList  *(*get_keys) ( ZyppUserRequest *self );
  gchar  *(*get_content_type)( ZyppUserRequest *self );
  void    (*set_accepted)( ZyppUserRequest *self );
  void    (*set_ignored) ( ZyppUserRequest *self );
  gboolean(*get_accepted)( ZyppUserRequest *self );
};


/**
 * ZyppUserRequest:
 *
 * `ZyppUserRequest`s are data structures, created by libzypp to represent direct requests to
 * the user. They are delivered via a signal in the currently active zypp context object.
 *
 * If a request requires a answer from the user, they always have a default response in case the progress is running in non interactive mode.
 */

ZyppUserRequestType zypp_user_request_get_request_type( ZyppUserRequest *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_user_request_get_data:
 *
 * Get's the corresponding request data value for key, if there is no such key NULL is returned.
 * See the user request documentation for a data field listing of libzypp's user requests.
 *
 * Returns: (nullable) (transfer full): The data type corresponding to the key.
 */
GValue *zypp_user_request_get_data ( ZyppUserRequest *self, const char *key  ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_user_request_get_keys:
 *
 * Returns: (element-type gchar) (transfer full): list of keys of userdata
 */
GList  *zypp_user_request_get_keys ( ZyppUserRequest *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_user_request_get_content_type:
 *
 * Returns: (nullable) (transfer full): Returns the content type string of the user request.
 */
gchar *zypp_user_request_get_content_type( ZyppUserRequest *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_user_request_set_accepted:
 *
 * Marks the user request as handled. Before handling a event code should always check if it
 * was already handled. If a request is not marked as handled, it will bubble up to the parent
 * task observer until being returned as unhandled to the calling code.
 */
void     zypp_user_request_set_accepted( ZyppUserRequest *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_user_request_set_ignored:
 *
 * Marks the user request as NOT handled, basically clearing the internal flag. As long as a request
 * is not marked as handled it will bubble up the task observer tree until either the root observer
 * also chose to ignore it, or it is marked as handled by user code.
 */
void     zypp_user_request_set_ignored ( ZyppUserRequest *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_user_request_get_accepted:
 *
 * Returns true if the request was already handled.
 */
gboolean zypp_user_request_get_accepted( ZyppUserRequest *self ) LIBZYPP_GLIB_EXPORT;


G_END_DECLS

#ifdef  __cplusplus
#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppUserRequest, zypp_user_request, ZYPP, USER_REQUEST )
#endif

#endif
