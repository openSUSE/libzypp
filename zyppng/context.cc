#include "context.h"
#include <zypp-core/zyppng/base/EventLoop>

struct _ZyppContext
{
  GObjectClass            parent_class;
};
G_DEFINE_TYPE(ZyppContext, zypp_context, G_TYPE_OBJECT)

/*
 * Method definitions.
 */
void zypp_context_init( ZyppContext *ctx )
{
}

void zypp_context_class_init( ZyppContextClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

}
