/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/userrequest_p.h"
#include <gobject/gvaluecollector.h>

G_DEFINE_INTERFACE (ZyppUserRequest, zypp_user_request, G_TYPE_OBJECT)
static void zypp_user_request_default_init ( ZyppUserRequestInterface *g_class )
{
  /* add properties and signals to the interface here */
}

ZyppUserRequestType zypp_user_request_get_request_type( ZyppUserRequest *self )
{
  g_return_val_if_fail (ZYPP_IS_USER_REQUEST (self), ZYPP_USER_REQUEST_INVALID);
  return ZYPP_USER_REQUEST_GET_IFACE (self)->get_request_type( self );
}

GValue *zypp_user_request_get_data ( ZyppUserRequest *self, const char *key  )
{
  g_return_val_if_fail (ZYPP_IS_USER_REQUEST (self), nullptr);
  return ZYPP_USER_REQUEST_GET_IFACE (self)->get_data( self, key );
}

GList  *zypp_user_request_get_keys ( ZyppUserRequest *self )
{
  g_return_val_if_fail (ZYPP_IS_USER_REQUEST (self), nullptr);
  return ZYPP_USER_REQUEST_GET_IFACE (self)->get_keys( self );
}

gchar *zypp_user_request_get_content_type( ZyppUserRequest *self )
{
  g_return_val_if_fail (ZYPP_IS_USER_REQUEST (self), nullptr);
  return ZYPP_USER_REQUEST_GET_IFACE (self)->get_content_type( self );
}

void zypp_user_request_set_accepted( ZyppUserRequest *self )
{
  g_return_if_fail (ZYPP_IS_USER_REQUEST (self) );
  ZYPP_USER_REQUEST_GET_IFACE (self)->set_accepted( self );
}

void zypp_user_request_set_ignored ( ZyppUserRequest *self )
{
  g_return_if_fail (ZYPP_IS_USER_REQUEST (self) );
  ZYPP_USER_REQUEST_GET_IFACE (self)->set_ignored( self );
}

gboolean zypp_user_request_get_accepted( ZyppUserRequest *self )
{
  g_return_val_if_fail (ZYPP_IS_USER_REQUEST (self), false );
  return ZYPP_USER_REQUEST_GET_IFACE (self)->get_accepted( self );
}
