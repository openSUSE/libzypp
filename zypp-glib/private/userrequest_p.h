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

#include <zyppng/userrequest.h>

G_BEGIN_DECLS

#define ZYPP_USER_REQUEST_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), ZYPP_TYPE_USER_REQUEST, ZyppUserRequestClass))

typedef struct _ZyppUserRequestClass   ZyppUserRequestClass;

/*< private >
 * ZyppUserRequest:
 * @ref_count: the reference count of the event
 * @event_type: the specialized event type
 *
 * The abstract type for all user request events.
 */
struct _ZyppUserRequest
{
  GTypeInstance parent_instance;

  grefcount ref_count;

  /* Specialised event type */
  ZyppUserRequestType event_type;
};

/*< private >
 * ZyppUserRequestClass:
 * @finalize: a function called when the last reference held on an user request is
 *   released; implementations of ZyppUserRequest must chain up to the parent class
 *
 * The base class for user requests.
 */
struct _ZyppUserRequestClass
{
  GTypeClass parent_class;

  void                  (* finalize)            ( ZyppUserRequest *event );
};


/*< private >
 * ZyppUserRequestTypeInfo:
 * @instance_size: the size of the instance of a ZyppUserRequest subclass
 * @instance_init: (nullable): the function to initialize the instance data
 * @finalize: (nullable): the function to free the instance data
 *
 * A structure used when registering a new ZyppUserRequest type.
 */
typedef struct {
  gsize instance_size;

  void (* instance_init) (ZyppUserRequest *event);
  void (* finalize) (ZyppUserRequest *event);

} ZyppUserRequestTypeInfo;

GType zypp_user_request_type_register_static (const char *type_name, const ZyppUserRequestTypeInfo *type_info);

/*< private >
 * ZYPP_DEFINE_USER_REQUEST_TYPE:
 * @TypeName: the type name, in camel case
 * @type_name: the type name, in snake case
 * @type_info: the address of the `ZyppUserRequest`TypeInfo for the event type
 * @_C_: custom code to call after registering the event type
 *
 * Registers a new `ZyppUserRequest` subclass with the given @TypeName and @type_info.
 *
 * Similarly to %G_DEFINE_TYPE_WITH_CODE, this macro will generate a `get_type()`
 * function that registers the event type.
 *
 * You can specify code to be run after the type registration; the `GType` of
 * the event is available in the `zypp_define_user_request_type_id` variable.
 */
#define ZYPP_DEFINE_USER_REQUEST_TYPE(TypeName, type_name, type_info, _C_) \
GType \
  type_name ## _get_type (void) \
{ \
    static gsize zypp_define_user_request_type_id__volatile; \
    if (g_once_init_enter (&zypp_define_user_request_type_id__volatile)) \
  { \
      GType zypp_define_user_request_type_id = \
      zypp_user_request_type_register_static (g_intern_static_string (#TypeName), type_info); \
    { _C_ } \
      g_once_init_leave (&zypp_define_user_request_type_id__volatile, zypp_define_user_request_type_id); \
  } \
    return zypp_define_user_request_type_id__volatile; \
}

#define ZYPP_USER_REQUEST_SUPER(request) \
((ZyppUserRequestClass *) g_type_class_peek (g_type_parent (G_TYPE_FROM_INSTANCE (request))))


G_END_DECLS

#endif
