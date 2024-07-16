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

G_BEGIN_DECLS


#define ZYPP_TYPE_USER_REQUEST          (zypp_user_request_get_type ())

#define ZYPP_IS_USER_REQUEST(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ZYPP_TYPE_USER_REQUEST))
#define ZYPP_USER_REQUEST(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), ZYPP_TYPE_USER_REQUEST, ZyppUserRequest))

typedef struct _ZyppUserRequest         ZyppUserRequest;
typedef struct _ZyppMediaChangeRequest  ZyppMediaChangeRequest;
typedef struct _ZyppPopupRequest        ZyppPopupRequest;


/**
 * ZyppUserRequestType:
 * @ZYPP_USER_REQUEST_POPUP: As user request that has a label and predefined possible answers
 * @ZYPP_USER_REQUEST_MEDIACHANGE: Ask the user to change a medium
 *
 * Specifies the type of the event.
 */
  typedef enum {
    ZYPP_USER_REQUEST_POPUP,
    ZYPP_USER_REQUEST_MEDIACHANGE,
    ZYPP_USER_REQUEST_LAST        /* helper variable for decls */
  } ZyppUserRequestType;

ZyppUserRequest *zypp_user_request_ref (ZyppUserRequest *request);
void zypp_user_request_unref (ZyppUserRequest *request);

GType zypp_user_request_get_type ( void );

ZyppUserRequestType zypp_user_request_get_request_type( ZyppUserRequest *self );

G_END_DECLS

#endif
