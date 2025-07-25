/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/parser/json.h
 *
*/
#ifndef ZYPP_CORE_PARSER_JSON_PARSER_H
#define ZYPP_CORE_PARSER_JSON_PARSER_H

#include <optional>
#include <zypp-core/parser/parseexception.h>
#include <zypp-core/base/inputstream.h>
#include <zypp-core/base/NonCopyable.h>
#include <zypp-core/ng/pipelines/expected.h>
#include "json/JsonValue.h"

namespace zypp::json {

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
}
#endif
