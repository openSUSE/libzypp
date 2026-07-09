/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-core/parser/json.h
 *
 * Public JSON API.
 *
 * External consumers (compiling without -DZYPP_DLL) see only:
 *   - zypp::json::Value, Object, Array and their supporting types
 *   - zypp::json::parseDocument() — throws parser::ParseException on error
 *
 * Code compiling as part of libzypp (-DZYPP_DLL) additionally gets:
 *   - zypp::json::parseDocumentExpected() — NG-style expected<Value> return
 *   - zypp::json::detail::Parser (via json/private/jsonparser_p.h) for
 *     direct tokeniser use
 */
#ifndef ZYPP_CORE_PARSER_JSON_H
#define ZYPP_CORE_PARSER_JSON_H

#include <zypp-core/Globals.h>
#include <zypp-core/base/InputStream>
#include <zypp-core/parser/parseexception.h>
#include "json/JsonValue.h"

#ifdef ZYPP_DLL
#  include <zypp-core/ng/pipelines/expected.h>
#  include "json/private/jsonparser_p.h"
#endif

namespace zypp::json {

  /** Parse a JSON document from \a input_r.
   * \throws parser::ParseException on malformed input.
   * Safe to use from any consumer — no NG types in this signature.
   */
  ZYPP_API Value parseDocument( const InputStream & input_r );

#ifdef ZYPP_DLL
  /** Parse a JSON document, returning expected<Value> instead of throwing.
   * Only available when building as part of libzypp (-DZYPP_DLL).
   * Not part of the external ABI — zyppng::expected's layout is not frozen
   * for third-party consumers.
   */
  zyppng::expected<Value> parseDocumentExpected( const InputStream & input_r );
#endif

} // namespace zypp::json

#endif // ZYPP_CORE_PARSER_JSON_H
