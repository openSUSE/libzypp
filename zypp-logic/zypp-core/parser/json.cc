/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "json.h"
#include <zypp-core/AutoDispose.h>

namespace zypp::json {

  constexpr std::string_view TOK_NAMES[Parser::Token::TOK_COUNT] = {
    "TOK_STRING",
    "TOK_NUMBER_FLOAT",
    "TOK_NUMBER_UINT",
    "TOK_NUMBER_INT",
    "TOK_BOOL_TRUE",
    "TOK_BOOL_FALSE",
    "TOK_NULL",
    "TOK_LSQUARE_BRACKET",
    "TOK_RSQUARE_BRACKET",
    "TOK_LCURLY_BRACKET",
    "TOK_RCURLY_BRACKET",
    "TOK_COMMA",
    "TOK_COLON",
    "TOK_END"
  };

  static bool isWhiteSpace ( const char ch ) {
    static constexpr char whitespaces[] { ' ', '\n', '\r', '\t', 0 };
    return ( std::find (whitespaces, whitespaces+4, ch ) != whitespaces+4 );
  }

  zyppng::expected<Value> Parser::parse( const InputStream &input_r )
  {
    using namespace zyppng::operators;
    _stream = input_r;
    zypp_defer{
      _stream.reset();
    };

    auto document = nextToken()
    | and_then( [&](Parser::Token t) {
      switch ( t._type ) {
        case Token::TOK_LSQUARE_BRACKET:
          return parseArray()  | and_then([&]( Array a ){ return zyppng::make_expected_success( Value(std::move(a))); } );
        case Token::TOK_LCURLY_BRACKET:
          return parseObject() | and_then([&]( Object a ){ return zyppng::make_expected_success( Value(std::move(a))); } );
        case Token::TOK_STRING:
          return zyppng::make_expected_success<Value>( String(t._token) );
        case Token::TOK_BOOL_FALSE:
          return zyppng::make_expected_success<Value>( Bool(false) );
        case Token::TOK_BOOL_TRUE:
          return zyppng::make_expected_success<Value>( Bool(true) );
        case Token::TOK_NUMBER_FLOAT:
          return Number::fromString( t._token ) | and_then( []( Number n ){ return zyppng::make_expected_success( Value(std::move(n)) ); } );
        case Token::TOK_NUMBER_UINT:
          return UInt::fromString( t._token )   | and_then( []( UInt n ){ return zyppng::make_expected_success( Value(std::move(n)) ); } );
        case Token::TOK_NUMBER_INT:
          return Int::fromString( t._token )    | and_then( []( Int n ){ return zyppng::make_expected_success( Value(std::move(n)) ); } );
        case Token::TOK_NULL:
          return zyppng::make_expected_success( Value() );
        case Token::TOK_END:
          return zyppng::make_expected_success( Value() );
        default:
          return makeParseError<Value>(  str::Str() << "Unexpected token " << TOK_NAMES[t._type] << ", json document must consist of a valid JSON value, object or array.", ZYPP_EX_CODELOCATION );
      }
    });

    if ( !document )
      return document;

    // expect end of document
    auto endToken = nextToken();
    if ( !endToken )
      return zyppng::expected<Value>::error( endToken.error() );
    if ( endToken->_type != Token::TOK_END ) {
      return makeParseError<Value>(  str::Str() << "Unexpected token " << TOK_NAMES[endToken->_type] << ", EOF was expected.", ZYPP_EX_CODELOCATION );
    }
    // all good
    return document;
  }

  zyppng::expected<Object> Parser::parseObject()
  {
    using namespace zyppng::operators;

    _nestingDepth++;
    zypp_defer{ _nestingDepth--; };
    if ( _nestingDepth > 1000 ) {
      return makeParseError<Object>(  str::Str() << "Nesting level is too deep ( > 1000 ).", ZYPP_EX_CODELOCATION );
    }

    enum State {
      Obj_Start,
      Obj_Expect_Comma_Or_End,
      Obj_Expect_Pair
    } state = Obj_Start;

    Object o;

    const auto &parsePair = [&]( String &&key ) {
      return nextToken()
      | and_then([&]( Token maybeColon ) {
        if ( maybeColon._type == Token::TOK_COLON )
          return parseValue();
        else
          return makeParseError<Value>(  str::Str() << "Unexpected token " << TOK_NAMES[maybeColon._type] << ", colon was expected in key, value pair.", ZYPP_EX_CODELOCATION );
      })
      | and_then([&]( Value val ){
        o.add ( key, std::move(val) );
        return zyppng::expected<void>::success ();
      });
    };

    while ( true ) {

      auto t = nextToken();
      if ( !t )
        return  zyppng::expected<Object>::error( t.error( ) );

      switch ( state ) {
        case Obj_Start: {
          // we can have either the start of a key-value pair or a closing }
          switch ( t->_type ) {
            case Token::TOK_STRING: {
              auto res = parsePair( String( std::move(t->_token) ) );
              if ( !res )
                return  zyppng::expected<Object>::error( res.error( ) );

              state = Obj_Expect_Comma_Or_End;
              break;
            }
            case Token::TOK_RCURLY_BRACKET: {
              return zyppng::make_expected_success(std::move(o));
              break;
            }
            default:
              return makeParseError<Object>(  str::Str() << "Unexpected token " << TOK_NAMES[t->_type] << " while parsing a Object.", ZYPP_EX_CODELOCATION );
              break;
          }
          break;
        }
        case Obj_Expect_Comma_Or_End: {
          switch ( t->_type ) {
            case Token::TOK_COMMA: {
              state = Obj_Expect_Pair;
              break;
            }
            case Token::TOK_RCURLY_BRACKET: {
              return zyppng::make_expected_success(std::move(o));
              break;
            }
            default:
              return makeParseError<Object>(  str::Str() << "Unexpected token " << TOK_NAMES[t->_type] << " while parsing a Object.", ZYPP_EX_CODELOCATION );
              break;
          }
          break;
        }
        case Obj_Expect_Pair: {
          // we can have either the start of a key-value pair or a closing }
          switch ( t->_type ) {
            case Token::TOK_STRING: {
              auto res = parsePair( String( std::move(t->_token) ) );
              if ( !res )
                return  zyppng::expected<Object>::error( res.error( ) );

              state = Obj_Expect_Comma_Or_End;
              break;
            }
            default:
              return makeParseError<Object>(  str::Str() << "Unexpected token " << TOK_NAMES[t->_type] << " while parsing a Object.", ZYPP_EX_CODELOCATION );
              break;
          }
          break;
        }
      }
    }
  }

  zyppng::expected<Array> Parser::parseArray()
  {
    using namespace zyppng::operators;

    _nestingDepth++;
    zypp_defer{ _nestingDepth--; };
    if ( _nestingDepth > 1000 ) {
      return makeParseError<Array>(  str::Str() << "Nesting level is too deep ( > 1000 ).", ZYPP_EX_CODELOCATION );
    }

    enum State {
      Arr_Init,   // can be value or array end token
      Arr_Value,  // next token must be a value
      Arr_Expect_Comma_Or_End, // next token must be TOK_COMMA or TOK_RSQUARE_BRACKET
    } state = Arr_Init;

    Array a;
    while ( true ) {
      switch ( state ) {
        case Arr_Init: {
          auto tok = nextToken();
          if ( !tok )
            return zyppng::expected<Array>::error( tok.error( ) );

          if ( tok->_type == Token::TOK_RSQUARE_BRACKET )
            return zyppng::make_expected_success( std::move(a) );

          auto maybeVal = finishParseValue( std::move(*tok) );
          if ( !maybeVal )
            return  zyppng::expected<Array>::error( maybeVal.error( ) );

          a.add( std::move(*maybeVal) );
          state = Arr_Expect_Comma_Or_End;
          break;
        }
        case Arr_Value: {
          auto maybeVal = parseValue();
          if ( !maybeVal )
            return  zyppng::expected<Array>::error( maybeVal.error( ) );

          a.add( std::move(*maybeVal) );
          state = Arr_Expect_Comma_Or_End;
          break;
        }
        case Arr_Expect_Comma_Or_End: {
          auto tok = nextToken();
          if ( !tok )
            return zyppng::expected<Array>::error( tok.error( ) );

          switch(tok->_type) {
            case Token::TOK_RSQUARE_BRACKET:
               return zyppng::make_expected_success( std::move(a) );
            case Token::TOK_COMMA:
              state = Arr_Value;
              break;
            default:
              return makeParseError<Array>(  str::Str() << "Unexpected token " << TOK_NAMES[tok->_type] << " while parsing a JSON value.", ZYPP_EX_CODELOCATION );
              break;
          }
          break;
        }
      }
    }
  }

  /*!
   * Parse a complete value from the stream
   */
  zyppng::expected<Value> Parser::parseValue()
  {
    using namespace zyppng::operators;
    return nextToken()
    | and_then([&]( Token &&tok ) {
      return finishParseValue ( std::move(tok) );
    });
  }

  /*!
   * Finish parsing a JSON value, the token passed in \a begin either IS
   * the value in case of essential types or the begin token of a array or object
   */
  zyppng::expected<Value> Parser::finishParseValue( Token begin )
  {
    using namespace zyppng::operators;
    switch( begin._type ) {
      case Token::TOK_STRING:
        return zyppng::make_expected_success( Value( String( std::move(begin._token) )));
      case Token::TOK_NUMBER_FLOAT:
        return Number::fromString( begin._token ) | and_then( []( Number n ){ return zyppng::make_expected_success( Value(std::move(n)) ); } );
      case Token::TOK_NUMBER_UINT:
        return UInt::fromString( begin._token )   | and_then( []( UInt n ){ return zyppng::make_expected_success( Value(std::move(n)) ); } );
      case Token::TOK_NUMBER_INT:
        return Int::fromString( begin._token )    | and_then( []( Int n ){ return zyppng::make_expected_success( Value(std::move(n)) ); } );
      case Token::TOK_BOOL_TRUE:
        return zyppng::make_expected_success( Value( Bool( true ) ));
      case Token::TOK_BOOL_FALSE:
        return zyppng::make_expected_success( Value( Bool( false ) ));
      case Token::TOK_NULL:
        return zyppng::make_expected_success( Value() );
      case Token::TOK_LSQUARE_BRACKET:
        return parseArray() | and_then( []( Array a ){ return zyppng::make_expected_success( Value(std::move(a)) ); } );
      case Token::TOK_LCURLY_BRACKET:
        return parseObject() | and_then( []( Object o ){ return zyppng::make_expected_success( Value(std::move(o)) ); } );
      default:
        return makeParseError<Value>(  str::Str() << "Unexpected token " << TOK_NAMES[begin._type] << " while parsing a JSON value.", ZYPP_EX_CODELOCATION );
        break;
    }
  }

  zyppng::expected<Parser::Token> Parser::parseNullToken()
  {
    using namespace zyppng::operators;
    return consumeString ( "null" )
    | and_then ( [&](){
      return zyppng::make_expected_success( Token{ ._type = Token::TOK_NULL } );
    });
  }

  zyppng::expected<Parser::Token> Parser::parseBoolToken()
  {
    using namespace zyppng::operators;
    char next = peekChar();
    if ( next == 't' ) {
      return consumeString ( "true" )
      | and_then ( [&](){
        return zyppng::make_expected_success( Token{ ._type = Token::TOK_BOOL_TRUE } );
      });
    } else if ( next == 'f' ) {
      return consumeString ( "false" )
      | and_then ( [&](){
        return zyppng::make_expected_success( Token{ ._type = Token::TOK_BOOL_FALSE } );
      });
    } else if ( next == eofChar() ) {
      return makeParseError(  str::Str() << "Unexpected EOF while parsing a boolean", ZYPP_EX_CODELOCATION );
    } else {
      return makeParseError(  str::Str() << "Unexpected char " << next << " while parsing a boolean", ZYPP_EX_CODELOCATION );
    }
  }

  zyppng::expected<Parser::Token> Parser::parseNumberToken()
  {
    enum State {
      Begin, // accepts sign, 0-9
      ExpectFraction_or_End, // received 0 in init state, now needs a fraction a exponent or an end
      ExpectDigit, // requires a digit to be next
      Digit, // expects digit 0-9 or e E to move into exponent parsing, any other char ends number parsing
      Exponent_Begin, // we saw a e or E and now expect a sign or digit
      Exponent_ExpectDigit, // state entered after a sign, where we require a digit
      Exponent // digits until we see something else
    } state = Begin;

    Token t;
    bool isSigned    = false;
    bool acceptSign  = true;
    bool done        = false;
    bool isFloating  = false;
    bool hasFraction = false;

    while ( !done ) {
      char c = peekChar();

      switch ( state ) {
        case Begin: {
          if ( c == '-' && acceptSign ) {
            acceptSign = false;
            isSigned   = true;
            t._token.push_back(c);
            consumeChar();
          } else if ( c >= '1' && c <= '9' ) {
            t._token.push_back (c);
            consumeChar();
            state = Digit;
          } else if ( c == '0' ) {
            t._token.push_back (c);
            consumeChar();
            state = ExpectFraction_or_End;
          } else if ( c == eofChar() ) {
            return makeParseError(  str::Str() << "Unexpected EOF while parsing a number.", ZYPP_EX_CODELOCATION );
          } else {
            return makeParseError(  str::Str() << "Unexpected char " << c << " while parsing a number.", ZYPP_EX_CODELOCATION );
          }
          break;
        }
        case ExpectFraction_or_End: {
          if ( c == '.' ) {
            t._token.push_back(c);
            consumeChar();
            isFloating = true;
            hasFraction = true;
            state = ExpectDigit;
          } else if ( c == 'e' || c == 'E' ) {
            t._token.push_back (c);
            consumeChar();
            state = Exponent_Begin;
            isFloating = true;
          } else {
            // any other char marks the end of the number
            done = true;
          }
          break;
        }
        case ExpectDigit: {
          if ( c >= '0' && c <= '9' ) {
            t._token.push_back (c);
            consumeChar();
            state = Digit;
          } else {
            // any other char is a error
            return makeParseError(  str::Str() << "Unexpected char " << c << " while parsing a number, digit was expected.", ZYPP_EX_CODELOCATION );
          }
          break;
        }
        case Digit: {
          if ( c >= '0' && c <= '9' ) {
            t._token.push_back (c);
            consumeChar();
          } else if ( c == 'e' || c == 'E' ) {
            t._token.push_back (c);
            consumeChar();
            state = Exponent_Begin;
            isFloating = true;
          } else if ( c == '.' && !hasFraction ) {
            t._token.push_back(c);
            consumeChar();
            isFloating = true;
            hasFraction = true;
            state = ExpectDigit;
          } else {
            // any other char marks the end of the number
            done = true;
          }
          break;
        }
        case Exponent_Begin: {
          if ( c == '+' || c == '-' ) {
            t._token.push_back (c);
            consumeChar();
            state = Exponent_ExpectDigit;
          } else if ( c >= '0' && c <= '9') {
            t._token.push_back (c);
            consumeChar();
            state = Exponent;
          } else if ( c == eofChar() ) {
            return makeParseError(  str::Str() << "Unexpected EOF while parsing a number.", ZYPP_EX_CODELOCATION );
          } else {
            return makeParseError(  str::Str() << "Unexpected char " << c << " while parsing a number, sign or digit was expected.", ZYPP_EX_CODELOCATION );
          }
          break;
        }
        case Exponent_ExpectDigit:
        case Exponent: {
          if ( c >= '0' && c <= '9' ) {

            if ( state != Exponent )
              state = Exponent;

            t._token.push_back (c);
            consumeChar();

          } else if ( state == Exponent_ExpectDigit ) {
            return makeParseError(  str::Str() << "Unexpected char " << c << " while parsing a number with exponent, digit was expected.", ZYPP_EX_CODELOCATION );
          } else {
            // any other char marks the end of the number
            done = true;
          }
          break;
        }
      }
    }

    if ( isFloating ) {
      t._type = Token::TOK_NUMBER_FLOAT;
    } else {
      if ( isSigned ) {
        t._type = Token::TOK_NUMBER_INT;
      } else {
        t._type = Token::TOK_NUMBER_UINT;
      }
    }
    return zyppng::make_expected_success( std::move(t) );
  }

  std::istream::char_type Parser::popChar()
  {
    if ( !_stream )
      return std::istream::traits_type::eof();

    auto &input = _stream->stream();
    if ( input.eof () )
      return std::istream::traits_type::eof();

    return input.get();
  }

  std::istream::char_type Parser::peekChar()
  {
    if ( !_stream )
      return std::istream::traits_type::eof();

    auto &input = _stream->stream();
    if ( input.eof () )
      return std::istream::traits_type::eof();

    return input.peek();
  }

  void Parser::consumeChar()
  {
    if ( _stream )
      _stream->stream().ignore();
  }

  zyppng::expected<void> Parser::consumeString( const std::string &str )
  {
    for ( uint i = 0; i < str.size(); i++ ) {
      char c = popChar();
      if ( c == eofChar () )
        return makeParseError<void>( str::Str() << "Unexpected EOF while parsing: " << str, ZYPP_EX_CODELOCATION );
      if ( c != str.at(i) )
        return makeParseError<void>( str::Str() << "Unexpected char " << c << " ,character " << str.at(i) << " was expected while parsing: " << str, ZYPP_EX_CODELOCATION );
    }
    return zyppng::expected<void>::success();
  }

  zyppng::expected<Parser::Token> Parser::nextToken()
  {
    try {
      char nextChar = peekChar();

      while ( nextChar != eofChar()
              && isWhiteSpace (nextChar) ) {
        consumeChar ();
        nextChar = peekChar();
      }

      if ( nextChar == eofChar() ) {
        return zyppng::make_expected_success( Token {
          ._type = Token::TOK_END
        } );
      }

      switch ( nextChar ) {
        case '"': {
          return parseStringToken ();
        }
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
          return parseNumberToken();
        }
        case 't':
        case 'f': {
          return parseBoolToken();
        }
        case 'n': {
          return parseNullToken();
        }
        case '{': {
          consumeChar ();
          return zyppng::make_expected_success( Token {
            ._type = Token::TOK_LCURLY_BRACKET
          } );
        }
        case '}': {
          consumeChar ();
          return zyppng::make_expected_success( Token {
            ._type = Token::TOK_RCURLY_BRACKET
          } );
        }
        case '[': {
          consumeChar ();
          return zyppng::make_expected_success( Token {
            ._type = Token::TOK_LSQUARE_BRACKET
          } );
        }
        case ']': {
          consumeChar ();
          return zyppng::make_expected_success( Token {
            ._type = Token::TOK_RSQUARE_BRACKET
          } );
        }
        case ':': {
          consumeChar ();
          return zyppng::make_expected_success( Token {
            ._type = Token::TOK_COLON
          } );
        }
        case ',': {
          consumeChar ();
          return zyppng::make_expected_success( Token {
            ._type = Token::TOK_COMMA
          } );
        }
        default:
          return makeParseError( ( str::Str() << "Unexpected token: " << nextChar << " in json stream."), ZYPP_EX_CODELOCATION );
          break;
      }
    } catch (...) {
      return zyppng::expected<Token>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
  }

  zyppng::expected<Parser::Token> Parser::parseStringToken()
  {
    using namespace zyppng::operators;
    auto c = popChar ();

    // shouldn't happen because we peeked before that there is a "
    if ( c != '"' ) {
      return makeParseError( ( str::Str() << "Unexpected token: " << c << ", a JSON String must start with \"."), ZYPP_EX_CODELOCATION );
    }

    Token t {
      ._type = Token::TOK_STRING
    };

    while( true ) {
      c = popChar();
      // escaped
      if ( c == eofChar () ) {
        return makeParseError( ( str::Str() << "Unexpected EOF token in the middle of a string."), ZYPP_EX_CODELOCATION );

      } else if ( c == '\\' ) {

        c = this->popChar();
        if ( c == eofChar() ) {
            return makeParseError( "Unexpected EOF in escaped string character.", ZYPP_EX_CODELOCATION );
        }

        switch ( c ) {
          case '"' :
          case '\\':
          case '/' :
          {
            t._token.push_back(c);
            break;
          }
          case 'b' : {
            t._token.push_back( '\b' );
            break;
          }
          case 'f' : {
            t._token.push_back( '\f' );
            break;
          }
          case 'n' : {
            t._token.push_back( '\n' );
            break;
          }
          case 'r' : {
            t._token.push_back( '\r' );
            break;
          }
          case 't' : {
            t._token.push_back( '\t' );
            break;
          }
          case 'u' : {
            const auto &pop4HexBytes = [&]( ) {
              std::vector<char> data( 4, '\0' );
              for (int i = 0; i < 4; i++) {
                  data[i] = this->popChar();
                  if ( data[i] == eofChar() ) {
                      return makeParseError<std::vector<char>>( "Unexpected EOF in \\u escaped string character.", ZYPP_EX_CODELOCATION );
                  }

                  if (   (data[i] >= '0' && data[i] <= '9' )
                      || (data[i] >= 'A' && data[i] <= 'F' )
                      || (data[i] >= 'a' && data[i] <= 'f' )
                      ) {
                      // accept char
                      continue;
                  }
                  return makeParseError<std::vector<char>>( str::Str() << "Unexpected token " << str::hexstring(c) << " in \\u escaped string character.", ZYPP_EX_CODELOCATION );
              }
              return zyppng::expected<std::vector<char>>::success( std::move(data) );
            };

            // check data and convert it to a codepoint, if data contains the
            // start of a surrogate pair, read the second value and return the resulting codepoint
            const auto &convertToCodepoint = [&]( std::vector<char> data ){
              // check if the codepoint is int the UTF-16 surrogate pair range,
              // if yes we need to fetch another 4 bits

              const auto data1View = std::string_view(data.data(), data.size());
              auto cp = str::hexCharToValue<uint32_t>( data1View );

              if ( !cp ) {
                return makeParseError<uint32_t>( str::Str() << "Invalid \\u escaped character: " << data1View , ZYPP_EX_CODELOCATION );
              }

              // UTF16 surrogate pair?
              if ( (*cp) >= 0xd800 && (*cp) <= 0xdbff ) {
                return consumeString ("\\u" ) // require another \u
                    | and_then( [&](){ return pop4HexBytes( ); })
                    | and_then( [&]( std::vector<char> data2 ){
                      // convert to codepoint
                      const auto data2View = std::string_view(data2.data(), data2.size());
                      const auto lowCp  = str::hexCharToValue<uint32_t>( data2View );

                      if ( !lowCp ) {
                        return makeParseError<uint32_t>( str::Str() << "Invalid \\u escaped character: " << data2View , ZYPP_EX_CODELOCATION );
                      }

                      if ( *lowCp < 0xDC00 || *lowCp > 0xDFFF) {
                        return makeParseError<uint32_t>( str::Str() << "Invalid UTF16 surrogate pair:  (" << data1View << "," << data2View <<")" , ZYPP_EX_CODELOCATION );
                      }

                      constexpr uint16_t low10BitMask = 0x3FF; // low 10 bits
                      uint32_t codepoint = (( (*cp) & low10BitMask ) << 10 ) | ( (*lowCp) & low10BitMask );
                      codepoint += 0x10000;
                      return zyppng::expected<uint32_t>::success( codepoint );
                    });

              } else {
                return zyppng::expected<uint32_t>::success( *cp );
              }
            };

            const auto &codepointToUtf8String = [&]( uint32_t cp ) {
              if ( cp == 0 ) {
                // json does allow a null character to be part of the string
                t._token += '\0';
                return zyppng::expected<void>::success();
              }
              const auto &conv = str::codepointToUtf8String( cp );
              if ( conv.size () == 0 ) {
                return makeParseError<void>( str::Str() << "Invalid codepoint in string " << cp, ZYPP_EX_CODELOCATION );
              }
              if ( !str::validateUtf8(conv) ) {
                return makeParseError<void>( str::Str() << "Invalid codepoint in string " << cp << " ,this is not a valid UTF-8 character.", ZYPP_EX_CODELOCATION );
              }
              t._token += conv;
              return zyppng::expected<void>::success();
            };

            auto res = pop4HexBytes()
                | and_then( convertToCodepoint )
                | and_then( codepointToUtf8String );

            if ( !res )
              return zyppng::expected<Parser::Token>::error(res.error());

            break;
          }
          default:
            return makeParseError( str::Str() << "Unexpected token " << str::hexstring(c) << " in \\ escaped string character.", ZYPP_EX_CODELOCATION );
        }
      } else if( c == '"' ) {
        break;
      } else if( c >= 0 && c <= 0x1f ) {
        return makeParseError( str::Str() << "Unescaped control character " << str::hexstring(c) << " in string is not allowed.", ZYPP_EX_CODELOCATION );
      } else {
        t._token.push_back(c);
      }
    }

    return zyppng::make_expected_success(t);
  }

  Parser::Token Parser::Token::eof()
  {
    return Token {
      ._type = Token::TOK_END
    };
  }


}


