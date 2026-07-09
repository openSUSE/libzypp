/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "json.h"
#include "json/private/jsonparser_p.h"

namespace zypp::json {

#ifdef ZYPP_DLL
  zyppng::expected<Value> parseDocumentExpected( const InputStream & input_r )
  {
    detail::Parser p;
    return p.parse( input_r );
  }
#endif

  Value parseDocument( const InputStream & input_r )
  {
    // json.cc is always compiled with ZYPP_DLL defined, so
    // parseDocumentExpected is always available here.
    return parseDocumentExpected( input_r ).unwrap();
  }

} // namespace zypp::json
