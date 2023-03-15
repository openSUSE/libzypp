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

/**
 * ZyppUserRequest: (ref-func zypp_user_request_ref) (unref-func zypp_user_request_unref)
 *
 * `ZyppUserRequest`s are data structures, created by libzypp to represent direct requests to
 * the user. They are delivered via a `ZyppTaskStatus` object that is passed into libzypp functions that
 * support progress tracking or need user responses.
 *
 * The events are automatically published to the top level `ZyppTaskStatus` to be handled, they always have
 * a default response in case the progress is running in non interactive mode.
 */

// ---- Begin of boilerplate code for glib fundamental types for GValue support, this is basically copied from GObject/GdkEvent, there is no other way to do this anyway ----

static void
value_user_request_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_user_request_free_value (GValue *value)
{
  if (value->data[0].v_pointer != NULL)
    zypp_user_request_unref ( static_cast<ZyppUserRequest*>(value->data[0].v_pointer));
}

static void
value_user_request_copy_value (const GValue *src,
  GValue       *dst)
{
  if (src->data[0].v_pointer != NULL)
    dst->data[0].v_pointer = zypp_user_request_ref (static_cast<ZyppUserRequest*>(src->data[0].v_pointer));
  else
    dst->data[0].v_pointer = NULL;
}

static gpointer
value_user_request_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static char *
value_user_request_collect_value (GValue      *value,
  guint        n_collect_values,
  GTypeCValue *collect_values,
  guint        collect_flags)
{
  ZyppUserRequest *event = static_cast<ZyppUserRequest *>(collect_values[0].v_pointer);

  if (event == NULL)
  {
    value->data[0].v_pointer = NULL;
    return NULL;
  }

  if (event->parent_instance.g_class == NULL)
    return g_strconcat ("invalid unclassed ZyppUserRequest pointer for "
                        "value type '",
      G_VALUE_TYPE_NAME (value),
      "'",
      NULL);

  value->data[0].v_pointer = zypp_user_request_ref (event);

  return NULL;
}

static char *
value_user_request_lcopy_value (const GValue *value,
  guint         n_collect_values,
  GTypeCValue  *collect_values,
  guint         collect_flags)
{
  ZyppUserRequest **event_p = static_cast<ZyppUserRequest **>(collect_values[0].v_pointer);

  if (event_p == NULL)
    return g_strconcat ("value location for '",
      G_VALUE_TYPE_NAME (value),
      "' passed as NULL",
      NULL);

  if (value->data[0].v_pointer == NULL)
    *event_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *event_p = static_cast<ZyppUserRequest *>(value->data[0].v_pointer);
  else
    *event_p = zypp_user_request_ref (static_cast<ZyppUserRequest*>(value->data[0].v_pointer));

  return NULL;
}


static void zypp_user_request_finalize ( ZyppUserRequest *self )
{
    g_type_free_instance ((GTypeInstance *) self);
}

static void zypp_user_request_class_init ( ZyppUserRequestClass *klass)
{
  klass->finalize = zypp_user_request_finalize;
}

static void zypp_user_request_init ( ZyppUserRequest *self )
{
  g_ref_count_init (&self->ref_count);
}

GType zypp_user_request_get_type (void)
{
  static gsize event_type__volatile;

  if (g_once_init_enter (&event_type__volatile)) {
    static const GTypeFundamentalInfo finfo = {
      static_cast<GTypeFundamentalFlags>(
        G_TYPE_FLAG_CLASSED |
        G_TYPE_FLAG_INSTANTIATABLE |
        G_TYPE_FLAG_DERIVABLE |
        G_TYPE_FLAG_DEEP_DERIVABLE),
      };

      static const GTypeValueTable value_table = {
        value_user_request_init,
        value_user_request_free_value,
        value_user_request_copy_value,
        value_user_request_peek_pointer,
        "p",
        value_user_request_collect_value,
        "p",
        value_user_request_lcopy_value,
        };

      const GTypeInfo event_info = {
        /* Class */
        sizeof (ZyppUserRequestClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) zypp_user_request_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,

        /* Instance */
        sizeof (ZyppUserRequest),
        0,
        (GInstanceInitFunc) zypp_user_request_init,

        /* GValue */
        &value_table,
        };

    GType event_type =
      g_type_register_fundamental (g_type_fundamental_next (),
        g_intern_static_string ("ZyppUserRequest"),
        &event_info, &finfo,
        G_TYPE_FLAG_ABSTRACT);

    g_once_init_leave (&event_type__volatile, event_type);
  }

  return event_type__volatile;
}

static void zypp_user_request_generic_class_init (gpointer g_class, gpointer class_data)
{
  ZyppUserRequestTypeInfo *info = static_cast<ZyppUserRequestTypeInfo *>(class_data);
  ZyppUserRequestClass *event_class = static_cast<ZyppUserRequestClass *>(g_class);

  /* Optional */
  if (info->finalize != NULL)
    event_class->finalize = info->finalize;

  g_free (info);
}

GType zypp_user_request_type_register_static (const char *type_name, const ZyppUserRequestTypeInfo *type_info)
{
  GTypeInfo info;

  info.class_size = sizeof (ZyppUserRequestClass);
  info.base_init = NULL;
  info.base_finalize = NULL;
  info.class_init = zypp_user_request_generic_class_init;
  info.class_finalize = NULL;
  info.class_data = g_memdup2 (type_info, sizeof (ZyppUserRequestTypeInfo));

  info.instance_size = type_info->instance_size;
  info.n_preallocs = 0;
  info.instance_init = (GInstanceInitFunc) type_info->instance_init;
  info.value_table = NULL;

  return g_type_register_static (ZYPP_TYPE_USER_REQUEST, type_name, &info, (GTypeFlags)0);
}

/**
 * zypp_user_request_ref:
 * @request: a `ZyppUserRequest`
 *
 * Increase the ref count of @event.
 *
 * Returns: (transfer full): @event
 */
ZyppUserRequest *zypp_user_request_ref (ZyppUserRequest *request)
{
  g_return_val_if_fail (ZYPP_IS_USER_REQUEST (request), nullptr);

  g_ref_count_inc (&request->ref_count);

  return request;
}

/**
 * zypp_user_request_unref:
 * @request: (transfer full): a `ZyppUserRequest`
 *
 * Decrease the ref count of @event.
 *
 * If the last reference is dropped, the structure is freed.
 */
void zypp_user_request_unref (ZyppUserRequest *request)
{
  g_return_if_fail (ZYPP_IS_USER_REQUEST (request));

  if (g_ref_count_dec (&request->ref_count))
    ZYPP_USER_REQUEST_GET_CLASS (request)->finalize (request);
}
