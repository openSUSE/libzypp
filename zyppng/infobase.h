/* zypp-info-base.h */

#ifndef _ZYPP_INFO_BASE_H
#define _ZYPP_INFO_BASE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define ZYPP_TYPE_INFO_BASE (zypp_info_base_get_type())
G_DECLARE_INTERFACE (ZyppInfoBase, zypp_info_base, ZYPP, INFO_BASE, GObject)

struct _ZyppInfoBaseInterface
{
  GTypeInterface parent_iface;

  gchar*    (*alias) (ZyppInfoBase *self);
  gchar*    (*escaped_alias) (ZyppInfoBase *self);
  gchar*    (*name)    (ZyppInfoBase *self);
  gchar*    (*raw_name) (ZyppInfoBase *self);
  gchar*    (*label) (ZyppInfoBase *self);
  gchar*    (*as_user_string) (ZyppInfoBase *self);
  gboolean  (*enabled) (ZyppInfoBase *self);
  gboolean  (*autorefresh) (ZyppInfoBase *self);
  gchar*    (*filepath) (ZyppInfoBase *self);

  void (*set_alias)   ( ZyppInfoBase *self, const gchar* alias );
  void (*set_name)    (ZyppInfoBase *self, const gchar *name );
  void (*set_enabled) (ZyppInfoBase *self, gboolean enabled );
  void (*set_autorefresh) (ZyppInfoBase *self, gboolean enabled );
  void (*set_filepath) (ZyppInfoBase *self, const gchar* filepath );
};

gchar*    zypp_info_base_alias (ZyppInfoBase *self);
gchar*    zypp_info_base_escaped_alias (ZyppInfoBase *self);
gchar*    zypp_info_base_name    (ZyppInfoBase *self);
gchar*    zypp_info_base_raw_name (ZyppInfoBase *self);
gchar*    zypp_info_base_label (ZyppInfoBase *self);
gchar*    zypp_info_base_as_user_string (ZyppInfoBase *self);
gboolean  zypp_info_base_enabled (ZyppInfoBase *self);
gboolean  zypp_info_base_autorefresh (ZyppInfoBase *self);
gchar*    zypp_info_base_filepath ( ZyppInfoBase *self );

void zypp_info_base_set_alias ( ZyppInfoBase *self, const gchar* alias );
void zypp_info_base_set_name    (ZyppInfoBase *self, const gchar *name );
void zypp_info_base_set_enabled (ZyppInfoBase *self, gboolean enabled );
void zypp_info_base_set_autorefresh (ZyppInfoBase *self, gboolean enabled );
void zypp_info_base_set_filepath (ZyppInfoBase *self, const gchar* filepath );

G_END_DECLS

#endif /* _ZYPP_INFO_BASE_H */
