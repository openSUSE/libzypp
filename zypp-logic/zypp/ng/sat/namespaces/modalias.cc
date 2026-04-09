/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "modalias.h"

#include <zypp-core/base/String.h>
#include <zypp/ng/idstring.h>

namespace zyppng::sat::namespaces {

  bool ModaliasNamespaceProvider::isSatisfied( detail::IdType value ) const
  {
    if ( !_query )
      return false;

    //@TODO this will use the target modalias implementation

    // modalias strings in capability may be hexencoded because rpm does not allow
    // ',', ' ' or other special chars.
    std::string decoded = zypp::str::hexdecode( IdString(value).asString() );
    return _query( decoded );
  }

}
