/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_GLIB_PRIVATE_INFOBASE_P_H
#define ZYPP_GLIB_PRIVATE_INFOBASE_P_H

#include <zypp-glib/infobase.h>

template <typename T, auto GlibTypeFun, auto GetPrivateFun >
struct ZyppInfoBaseImpl
{
  static gboolean T_IS_EXPECTED_GLIBTYPE (gpointer ptr) {
    return ZYPP_IS_INFO_BASE(ptr) &&  G_TYPE_CHECK_INSTANCE_TYPE ( ptr, GlibTypeFun() );
  }

  // make sure to call T_IS_EXPECTED_GLIBTYPE before calling this
  static auto T_GET_PRIVATE( ZyppInfoBase *self ) {
    return GetPrivateFun( G_TYPE_CHECK_INSTANCE_CAST (self, GlibTypeFun(), T) );
  }

  static gchar* zypp_info_base_alias ( ZyppInfoBase *self )
  {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self), nullptr );
    return g_strdup( T_GET_PRIVATE( self )->_info.alias().c_str() );
  }

  static gchar* zypp_info_base_escaped_alias (ZyppInfoBase *self)
  {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self), nullptr );
    return g_strdup( T_GET_PRIVATE( self )->_info.escaped_alias().c_str() );
  }

  static gchar* zypp_info_base_name (ZyppInfoBase *self)
  {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self), nullptr );
    return g_strdup( T_GET_PRIVATE( self )->_info.name().c_str() );
  }

  static gchar* zypp_info_base_raw_name (ZyppInfoBase *self)
  {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self), nullptr );
    return g_strdup( T_GET_PRIVATE( self )->_info.rawName().c_str() );
  }

  static gchar* zypp_info_base_label (ZyppInfoBase *self)
  {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self), nullptr );
    return g_strdup( T_GET_PRIVATE( self )->_info.label().c_str() );
  }

  static gchar* zypp_info_base_as_user_string (ZyppInfoBase *self)
  {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self), nullptr );
    return g_strdup( T_GET_PRIVATE( self )->_info.asUserString().c_str() );
  }

  static gboolean zypp_info_base_enabled (ZyppInfoBase *self)
  {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self), false );
    return T_GET_PRIVATE( self )->_info.enabled();
  }

  static gboolean zypp_info_base_autorefresh (ZyppInfoBase *self)
  {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self), false );
    return T_GET_PRIVATE( self )->_info.autorefresh();
  }

  static gchar* zypp_info_base_filepath ( ZyppInfoBase *self )
  {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self), nullptr );
    return g_strdup( T_GET_PRIVATE( self )->_info.filepath().c_str() );
  }

  static void zypp_info_base_set_alias ( ZyppInfoBase *self, const gchar* alias )
  {
    g_return_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) );
    T_GET_PRIVATE( self )->_info.setAlias( alias );
  }

  static void zypp_info_base_set_name    ( ZyppInfoBase *self, const gchar *name )
  {
    g_return_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) );
    T_GET_PRIVATE( self )->_info.setName( name );
  }

  static void zypp_info_base_set_enabled ( ZyppInfoBase *self, gboolean enabled )
  {
    g_return_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) );
    T_GET_PRIVATE( self )->_info.setEnabled( enabled );
  }

  static void zypp_info_base_set_autorefresh ( ZyppInfoBase *self, gboolean enabled )
  {
    g_return_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) );
    T_GET_PRIVATE( self )->_info.setAutorefresh( enabled );
  }

  static void zypp_info_base_set_filepath ( ZyppInfoBase *self, const gchar* filepath )
  {
    g_return_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) );
    T_GET_PRIVATE( self )->_info.setFilepath( filepath );
  }

  static void init_interface( ZyppInfoBaseInterface *iface )
  {
    iface->alias = zypp_info_base_alias;
    iface->escaped_alias = zypp_info_base_escaped_alias;
    iface->name = zypp_info_base_name;
    iface->raw_name = zypp_info_base_raw_name;
    iface->label = zypp_info_base_label;
    iface->as_user_string = zypp_info_base_as_user_string;
    iface->enabled = zypp_info_base_enabled;
    iface->autorefresh = zypp_info_base_autorefresh;
    iface->filepath = zypp_info_base_filepath;

    iface->set_alias = zypp_info_base_set_alias;
    iface->set_name = zypp_info_base_set_name;
    iface->set_enabled = zypp_info_base_set_enabled;
    iface->set_autorefresh = zypp_info_base_set_autorefresh;
    iface->set_filepath = zypp_info_base_set_filepath;
  }

};


#endif // INFOBASE_P_H
