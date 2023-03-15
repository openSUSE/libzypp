/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/* zypp-info-base.c */

#include "infobase.h"

G_DEFINE_INTERFACE (ZyppInfoBase, zypp_info_base, G_TYPE_OBJECT)
static void zypp_info_base_default_init ( ZyppInfoBaseInterface *g_class )
{
  /* add properties and signals to the interface here */
}

gchar *zypp_info_base_alias ( ZyppInfoBase *self )
{
  g_return_val_if_fail (ZYPP_IS_INFO_BASE (self), nullptr);
  return ZYPP_INFO_BASE_GET_IFACE (self)->alias( self );
}

gchar* zypp_info_base_escaped_alias (ZyppInfoBase *self)
{
  g_return_val_if_fail (ZYPP_IS_INFO_BASE (self), nullptr);
  return ZYPP_INFO_BASE_GET_IFACE (self)->escaped_alias( self );
}

gchar* zypp_info_base_name    (ZyppInfoBase *self)
{
  g_return_val_if_fail (ZYPP_IS_INFO_BASE (self), nullptr);
  return ZYPP_INFO_BASE_GET_IFACE (self)->name( self );
}

gchar* zypp_info_base_raw_name (ZyppInfoBase *self)
{
  g_return_val_if_fail (ZYPP_IS_INFO_BASE (self), nullptr);
  return ZYPP_INFO_BASE_GET_IFACE (self)->raw_name( self );
}

gchar* zypp_info_base_label (ZyppInfoBase *self)
{
  g_return_val_if_fail (ZYPP_IS_INFO_BASE (self), nullptr);
  return ZYPP_INFO_BASE_GET_IFACE (self)->label( self );
}

gchar* zypp_info_base_as_user_string (ZyppInfoBase *self)
{
  g_return_val_if_fail (ZYPP_IS_INFO_BASE (self), nullptr);
  return ZYPP_INFO_BASE_GET_IFACE (self)->as_user_string( self );
}

gboolean zypp_info_base_enabled (ZyppInfoBase *self)
{
  g_return_val_if_fail (ZYPP_IS_INFO_BASE (self), false);
  return ZYPP_INFO_BASE_GET_IFACE (self)->enabled( self );
}

gboolean zypp_info_base_autorefresh (ZyppInfoBase *self)
{
  g_return_val_if_fail (ZYPP_IS_INFO_BASE (self), false);
  return ZYPP_INFO_BASE_GET_IFACE (self)->autorefresh( self );
}

gchar* zypp_info_base_filepath (ZyppInfoBase *self)
{
  g_return_val_if_fail (ZYPP_IS_INFO_BASE (self), nullptr);
  return ZYPP_INFO_BASE_GET_IFACE (self)->filepath( self );
}

void zypp_info_base_set_alias ( ZyppInfoBase *self, const gchar* alias )
{
  g_return_if_fail (ZYPP_IS_INFO_BASE (self) );
  ZYPP_INFO_BASE_GET_IFACE (self)->set_alias( self, alias );
}

void zypp_info_base_set_name    (ZyppInfoBase *self, const gchar *name )
{
  g_return_if_fail (ZYPP_IS_INFO_BASE (self) );
  ZYPP_INFO_BASE_GET_IFACE (self)->set_name( self, name );
}

void zypp_info_base_set_enabled (ZyppInfoBase *self, gboolean enabled )
{
  g_return_if_fail (ZYPP_IS_INFO_BASE (self) );
  ZYPP_INFO_BASE_GET_IFACE (self)->set_enabled( self, enabled );
}

void zypp_info_base_set_autorefresh (ZyppInfoBase *self, gboolean enabled )
{
  g_return_if_fail (ZYPP_IS_INFO_BASE (self) );
  ZYPP_INFO_BASE_GET_IFACE (self)->set_autorefresh( self, enabled );
}

void zypp_info_base_set_filepath (ZyppInfoBase *self, const gchar* filepath )
{
  g_return_if_fail (ZYPP_IS_INFO_BASE (self) );
  ZYPP_INFO_BASE_GET_IFACE (self)->set_filepath( self, filepath );
}
