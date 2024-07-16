/* zypp-info-base.h */

#ifndef _ZYPP_INFO_BASE_H
#define _ZYPP_INFO_BASE_H

#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

#define ZYPP_TYPE_INFO_BASE (zypp_info_base_get_type())

#pragma GCC visibility push(default)
G_DECLARE_INTERFACE (ZyppInfoBase, zypp_info_base, ZYPP, INFO_BASE, GObject)
#pragma GCC visibility pop

struct LIBZYPP_GLIB_EXPORT _ZyppInfoBaseInterface
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

gchar*    zypp_info_base_alias (ZyppInfoBase *self) LIBZYPP_GLIB_EXPORT;
gchar*    zypp_info_base_escaped_alias (ZyppInfoBase *self) LIBZYPP_GLIB_EXPORT;
gchar*    zypp_info_base_name    (ZyppInfoBase *self) LIBZYPP_GLIB_EXPORT;
gchar*    zypp_info_base_raw_name (ZyppInfoBase *self) LIBZYPP_GLIB_EXPORT;
gchar*    zypp_info_base_label (ZyppInfoBase *self) LIBZYPP_GLIB_EXPORT;
gchar*    zypp_info_base_as_user_string (ZyppInfoBase *self) LIBZYPP_GLIB_EXPORT;
gboolean  zypp_info_base_enabled (ZyppInfoBase *self) LIBZYPP_GLIB_EXPORT;
gboolean  zypp_info_base_autorefresh (ZyppInfoBase *self) LIBZYPP_GLIB_EXPORT;
gchar*    zypp_info_base_filepath ( ZyppInfoBase *self ) LIBZYPP_GLIB_EXPORT;

void zypp_info_base_set_alias ( ZyppInfoBase *self, const gchar* alias ) LIBZYPP_GLIB_EXPORT;
void zypp_info_base_set_name    (ZyppInfoBase *self, const gchar *name ) LIBZYPP_GLIB_EXPORT;
void zypp_info_base_set_enabled (ZyppInfoBase *self, gboolean enabled ) LIBZYPP_GLIB_EXPORT;
void zypp_info_base_set_autorefresh (ZyppInfoBase *self, gboolean enabled ) LIBZYPP_GLIB_EXPORT;
void zypp_info_base_set_filepath (ZyppInfoBase *self, const gchar* filepath ) LIBZYPP_GLIB_EXPORT;

G_END_DECLS

#endif /* _ZYPP_INFO_BASE_H */
