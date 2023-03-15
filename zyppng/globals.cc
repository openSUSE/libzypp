/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "private/globals_p.h"
#include <zypp-core/zyppng/base/base.h>

void zyppng::internal::registerWrapper(const BaseRef &ref, GObject *wrapper)
{
  if ( ref->data ( ZYPP_WRAPPED_OBJ ) != nullptr )
    g_error( "Overriding a existing wrapper, this is a bug!" );
  ref->setData( ZYPP_WRAPPED_OBJ, zypp::AutoDispose<void*>(static_cast<void*>(wrapper)) );
}

void zyppng::internal::unregisterWrapper(const BaseRef &ref, GObject *wrapper)
{
  if ( ref->data ( ZYPP_WRAPPED_OBJ ) == wrapper )
    ref->clearData ( ZYPP_WRAPPED_OBJ );
}

G_DEFINE_QUARK(zypp-wrapped-obj-quark, zypp_wrapped_obj)
