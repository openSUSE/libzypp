/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_SHOW_MESSAGE_REQUEST_H
#define ZYPP_GLIB_SHOW_MESSAGE_REQUEST_H

#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>
#include <zypp-glib/ui/userrequest.h>

G_BEGIN_DECLS

#pragma GCC visibility push(default)
G_DECLARE_FINAL_TYPE ( ZyppShowMsgRequest, zypp_show_msg_request, ZYPP, SHOW_MSG_REQUEST, GObject )
#pragma GCC visibility pop

#define ZYPP_TYPE_SHOW_MSG_REQUEST (zypp_show_msg_request_get_type ())

/**
 * ZyppShowMsgRequestType:
 * @ZYPP_SHOW_MSG_TYPE_DEBUG: Show a debug level message
 * @ZYPP_SHOW_MSG_TYPE_INFO: Show a info level message
 * @ZYPP_SHOW_MSG_TYPE_WARNING: Show a warning level message
 * @ZYPP_SHOW_MSG_TYPE_ERROR: Show a error level message
 * @ZYPP_SHOW_MSG_TYPE_IMPORTANT: Show a important level message
 * @ZYPP_SHOW_MSG_TYPE_DATA: Show a data level message
 *
 * Specifies the type of message to be shown
 */
typedef enum {
  ZYPP_SHOW_MSG_TYPE_DEBUG,
  ZYPP_SHOW_MSG_TYPE_INFO,
  ZYPP_SHOW_MSG_TYPE_WARNING,
  ZYPP_SHOW_MSG_TYPE_ERROR,
  ZYPP_SHOW_MSG_TYPE_IMPORTANT,
  ZYPP_SHOW_MSG_TYPE_DATA
} ZyppShowMsgRequestType;

/**
 * ZyppShowMsgRequest:
 *
 * Signals the application to show a message to the user, which does not require
 * a answer. This could be implemented as a popup, or simply printed to a output window.
 */

ZyppShowMsgRequestType zypp_show_msg_request_get_message_type ( ZyppShowMsgRequest *self ) LIBZYPP_GLIB_EXPORT;
const char * zypp_show_msg_request_get_message( ZyppShowMsgRequest *self ) LIBZYPP_GLIB_EXPORT;

G_END_DECLS

#ifdef  __cplusplus
#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppShowMsgRequest, zypp_show_msg_request, ZYPP, SHOW_MSG_REQUEST )
#endif

#endif // ZYPP_GLIB_SHOW_MESSAGE_REQUEST_H
