/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_USERREQUEST_P_H
#define ZYPP_GLIB_USERREQUEST_P_H

#include <zypp-glib/ui/userrequest.h>
#include <zypp-glib/private/globals_p.h>

#include <zypp-core/UserData.h>
#include <zypp-glib/utils/GList>

template <typename T, auto GlibTypeFun, auto GetPrivateFun >
struct ZyppUserRequestImpl
{
  static gboolean T_IS_EXPECTED_GLIBTYPE (gpointer ptr) {
    return ZYPP_IS_USER_REQUEST(ptr) &&  G_TYPE_CHECK_INSTANCE_TYPE ( ptr, GlibTypeFun() );
  }

  // make sure to call T_IS_EXPECTED_GLIBTYPE before calling this
  static auto T_GET_PRIVATE( ZyppUserRequest *self ) {
    return GetPrivateFun( G_TYPE_CHECK_INSTANCE_CAST (self, GlibTypeFun(), T) );
  }

  static ZyppUserRequestType get_request_type( ZyppUserRequest *self ) {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) && T_GET_PRIVATE(self)->_cppObj, ZYPP_USER_REQUEST_INVALID );
    return static_cast<ZyppUserRequestType>( T_GET_PRIVATE(self)->_cppObj->type() );
  }

  static GValue * get_data( ZyppUserRequest *self, const char *key ) {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) && T_GET_PRIVATE(self)->_cppObj, nullptr );
    const zypp::callback::UserData &data = T_GET_PRIVATE(self)->_cppObj->userData();
    g_return_val_if_fail ( data.haskey(key), nullptr );

    const boost::any &val = data.getvalue(key);
    if ( val.type() == boost::typeindex::type_id<std::string>() ) {
      GValue *value = g_new0 (GValue, 1);
      g_value_init(value, G_TYPE_STRING );
      g_value_set_string(value, g_strdup( boost::any_cast<std::string>( val ).c_str() ) );
      return value;
    } else if( val.type() == boost::typeindex::type_id<int>() ) {
      GValue *value = g_new0 (GValue, 1);
      g_value_init(value, G_TYPE_INT );
      g_value_set_int( value, boost::any_cast<int>( val ) );
      return value;
    } else if( val.type() == boost::typeindex::type_id<unsigned int>() ) {
      GValue *value = g_new0 (GValue, 1);
      g_value_init(value, G_TYPE_INT );
      g_value_set_int( value, boost::any_cast<unsigned int>( val ) );
      return value;
    } else if( val.type() == boost::typeindex::type_id<zypp::Pathname>() ) {
      GValue *value = g_new0 (GValue, 1);
      g_value_init(value, G_TYPE_STRING );
      g_value_set_string(value, g_strdup( boost::any_cast<zypp::Pathname>( val ).c_str() ) );
      return value;
    }

    g_warning( ( zypp::str::Str()<<"Unknown value type for key: "<<key ).asString().c_str() );
    return nullptr;
  }

  static GList  * get_keys ( ZyppUserRequest *self ) {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) && T_GET_PRIVATE(self)->_cppObj, nullptr );

    zypp::glib::GCharContainer list;
    const zypp::callback::UserData &data = T_GET_PRIVATE(self)->_cppObj->userData();
    for ( const auto &entry : data.data() ) {
      list.push_back( g_strdup( entry.first.c_str () ) );
    }
    return list.take ();
  }

  static gchar *get_content_type( ZyppUserRequest *self ) {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) && T_GET_PRIVATE(self)->_cppObj, nullptr );
    const zypp::callback::UserData &data = T_GET_PRIVATE(self)->_cppObj->userData();
    return g_strdup(data.type().asString().c_str());
  }

  static void set_accepted( ZyppUserRequest *self )
  {
    g_return_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) && T_GET_PRIVATE(self)->_cppObj );
    T_GET_PRIVATE(self)->_cppObj->accept();
  }

  static void set_ignored ( ZyppUserRequest *self )
  {
    g_return_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) && T_GET_PRIVATE(self)->_cppObj );
    T_GET_PRIVATE(self)->_cppObj->ignore();
  }

  static gboolean get_accepted( ZyppUserRequest *self )
  {
    g_return_val_if_fail ( T_IS_EXPECTED_GLIBTYPE (self) && T_GET_PRIVATE(self)->_cppObj, false );
    return T_GET_PRIVATE(self)->_cppObj->accepted();
  }

  static void init_interface( ZyppUserRequestInterface *iface )
  {
    iface->get_request_type = get_request_type;
    iface->get_data = get_data;
    iface->get_keys = get_keys;
    iface->get_content_type = get_content_type;
    iface->set_accepted = set_accepted;
    iface->set_ignored = set_ignored;
    iface->get_accepted = get_accepted;
  }
};

#endif
