/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp-core/parser/json/private/jsonparser_p.h
 *
 * Internal JSON tokeniser — NOT part of the public API.
 * Include <zypp-core/parser/json.h> instead.
*/
#ifndef ZYPP_CORE_PARSER_JSON_PARSER_H
#define ZYPP_CORE_PARSER_JSON_PARSER_H

#include <optional>
#include <zypp-core/parser/parseexception.h>
#include <zypp-core/base/inputstream.h>
#include <zypp-core/base/NonCopyable.h>
#include <zypp-core/ng/pipelines/expected.h>
#include <zypp-core/ng/pipelines/mtry.h>
#include <zypp-core/Globals.h>
#include "../JsonValue.h"

namespace zypp::json::detail {

  /** fromString helpers — only callable from within the tokeniser.
   *  Moved here from JsonNumber.h to keep NG includes out of public headers. */
  inline zyppng::expected<Number> numberFromString( const std::string & str ) {
    using namespace zyppng::operators;
    using zyppng::operators::operator|;
    return zyppng::mtry( [&](){ return std::stod( str, nullptr ); } )
    | and_then( []( double res ) -> zyppng::expected<Number> { return zyppng::make_expected_success( Number(res) ); } );
  }

  inline zyppng::expected<Int> intFromString( const std::string & str ) {
    using namespace zyppng::operators;
    using zyppng::operators::operator|;
    return zyppng::mtry( [&](){ return std::stoll( str, nullptr ); } )
    | and_then( []( std::int64_t res ) { return zyppng::make_expected_success( Int(res) ); } );
  }

  inline zyppng::expected<UInt> uintFromString( const std::string & str ) {
    using namespace zyppng::operators;
    using zyppng::operators::operator|;
    return zyppng::mtry( [&](){ return std::stoull( str, nullptr ); } )
    | and_then( []( unsigned long long res ) { return zyppng::make_expected_success( UInt(res) ); } );
  }

  // Parser is an internal implementation detail — no ZYPP_API.
  // External code uses zypp::json::parseDocument() / parseDocumentExpected().
  class Parser : private base::NonCopyable
  {
  public:
    /** Default ctor */
    Parser() = default;
    Parser(const Parser &) = delete;
    Parser(Parser &&) = delete;
    Parser &operator=(const Parser &) = delete;
    Parser &operator=(Parser &&) = delete;

    /** Dtor */
    virtual ~Parser(){}

    /** Parse the stream.
     * \return ParseException on errors.
    */
    zyppng::expected<Value> parse( const InputStream & input_r );


    struct Token {

      enum Type {
        TOK_STRING = 0,
        TOK_NUMBER_FLOAT,
        TOK_NUMBER_UINT,
        TOK_NUMBER_INT,
        TOK_BOOL_TRUE,
        TOK_BOOL_FALSE,
        TOK_NULL,
        TOK_LSQUARE_BRACKET,
        TOK_RSQUARE_BRACKET,
        TOK_LCURLY_BRACKET,
        TOK_RCURLY_BRACKET,
        TOK_COMMA,
        TOK_COLON,
        TOK_END,  // end of document
        TOK_COUNT
      };

      Type _type;
      std::string _token;

      static Token eof();
    };

    zyppng::expected<Token> nextToken();

  private:

    zyppng::expected<Object> parseObject();
    zyppng::expected<Array>  parseArray();
    zyppng::expected<Value>  parseValue();
    zyppng::expected<Value>  finishParseValue( Token begin );


    zyppng::expected<Token> parseStringToken();
    zyppng::expected<Token> parseNumberToken();
    zyppng::expected<Token> parseBoolToken();
    zyppng::expected<Token> parseNullToken();


    std::istream::char_type popChar();
    std::istream::char_type peekChar();
    void consumeChar();

    zyppng::expected<void> consumeString( const std::string &str );

    static inline std::istream::char_type eofChar() {
      return std::istream::traits_type::eof();
    }

    template <typename T = Token>
    zyppng::expected<T> makeParseError( const std::string &message, exception_detail::CodeLocation &&loc )
    {
      return zyppng::expected<T>::error(::zypp::exception_detail::do_ZYPP_EXCPT_PTR(  parser::ParseException( message ), std::move(loc) ));
    }

    std::optional<InputStream> _stream;
    int _nestingDepth = 0; //< how deep is the nesting ( prevent memory overflow by too deep nesting )
  };

} // namespace zypp::json::detail
#endif
