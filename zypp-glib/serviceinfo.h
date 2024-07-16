#ifndef __ZYPP_SERVICE_INFO_H__
#define __ZYPP_SERVICE_INFO_H__

#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

typedef struct _ZyppServiceInfo ZyppServiceInfo;
typedef struct _ZyppContext ZyppContext;

#define ZYPP_TYPE_SERVICE_INFO (zypp_service_info_get_type ())
#pragma GCC visibility push(default)
G_DECLARE_FINAL_TYPE ( ZyppServiceInfo, zypp_service_info, ZYPP, SERVICE_INFO, GObject )
#pragma GCC visibility pop

ZyppServiceInfo *zypp_service_info_new ( ZyppContext *context ) LIBZYPP_GLIB_EXPORT;

G_END_DECLS

#ifdef  __cplusplus
#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppServiceInfo, zypp_service_info, ZYPP, SERVICE_INFO )
#endif

#endif /* __ZYPP_SERVICE_INFO_H__ */
