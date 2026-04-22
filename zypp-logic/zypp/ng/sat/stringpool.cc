/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "stringpool.h"

#include <zypp-core/base/Exception.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/base/LogTools.h>

namespace zyppng::sat {

  StringPool &StringPool::instance()
  {
    static StringPool p;
    return p;
  }

  StringPool::~StringPool()
  {
    ::pool_free( _pool );
  }

  StringPool::StringPool() : _pool( ::pool_create() )
  {
    MIL << "Creating sat-pool." << std::endl;
    if ( ! _pool )
    {
      ZYPP_THROW( zypp::Exception( _("Can not create sat-pool.") ) );
    }
    // libzypp#726: If the disttype is unset, ::pool_evrcmp_str uses
    // the default flavor set at compiletime. But even on DEBIAN we
    // handle rpm packages, so their rules must be applied.
    ::pool_setdisttype( _pool, DISTTYPE_RPM );
  }

}
