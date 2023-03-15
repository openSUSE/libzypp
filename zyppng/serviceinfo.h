#ifndef __ZYPP_SERVICE_INFO_H__
#define __ZYPP_SERVICE_INFO_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ZyppServiceInfo ZyppServiceInfo;

#define ZYPP_TYPE_SERVICE_INFO (zypp_service_info_get_type ())
G_DECLARE_FINAL_TYPE ( ZyppServiceInfo, zypp_service_info, ZYPP, SERVICE_INFO, GObject )

ZyppServiceInfo *zypp_service_info_new (void);

G_END_DECLS

#ifdef  __cplusplus
#include <zyppng/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppServiceInfo, zypp_service_info, ZYPP, SERVICE_INFO )
#endif

#endif /* __ZYPP_SERVICE_INFO_H__ */
