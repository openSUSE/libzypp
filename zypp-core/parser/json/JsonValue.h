/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_CORE_PARSER_JSON_JSON_VALUE_DEFINED
#define ZYPP_CORE_PARSER_JSON_JSON_VALUE_DEFINED

#include <cmath>
#include <variant>
#include <vector>
#include <map>
#include <set>
#include <ostream>

#include <zypp-core/base/String.h>

#include "JsonBool.h"
#include "JsonNull.h"
#include "JsonNumber.h"
#include "JsonString.h"

namespace zypp::json {

  class Value;
  class Array;
  class Object;

  class Array {

  public:

    using iterator = std::vector<Value>::iterator;
    using const_iterator = std::vector<Value>::const_iterator;
    using size_type = std::vector<Value>::size_type;

    Array() {}

    /** Construct from container iterator */
    template <class Iterator>
    Array( Iterator begin, Iterator end );

    template< class V>
    Array( const std::vector<V> & cont_r ) : Array(cont_r.begin (), cont_r.end()) {}

    template< class V>
    Array( const std::list<V> & cont_r ) : Array(cont_r.begin (), cont_r.end()) {}

    template< class V>
    Array( const std::set<V> & cont_r ) : Array(cont_r.begin (), cont_r.end()) {}

    /** Construct from container initializer list { v1, v2,... } */
    Array( std::initializer_list<Value> contents_r );

    /** Push JSON Value to Array */
    void add( Value val_r );

    /** \overload from container initializer list { v1, v2,... } */
    void add( const std::initializer_list<Value> & contents_r );

    /** JSON representation */
    std::string asJSON() const
    { return str::Str() << *this; }

    /** String representation */
    std::string asString() const
    { return asJSON(); }

    /** Stream output */
    std::ostream & dumpOn( std::ostream & str ) const;

    iterator begin() { return _values.begin (); }
    iterator end() { return _values.end (); }

    const_iterator begin() const { return _values.begin (); }
    const_iterator end() const { return _values.end (); }

    const Value &operator[]( size_type n ) const { return _values[n]; }
    Value &operator[]( size_type n ){ return _values[n]; }

    size_type size() const { return _values.size(); }

    bool operator==( const Array &other ) const;

  private:
    std::vector<Value> _values;
  };

  /** \relates Array Stream output */
  inline std::ostream & operator<<( std::ostream & str, const Array & obj )
  {
    return obj.dumpOn( str );
  }


  class Object {
  public:

    using iterator = std::multimap<String, Value>::iterator;
    using const_iterator = std::multimap<String, Value>::const_iterator;
    using size_type = std::multimap<String, Value>::size_type;

    Object();

    /** Construct from map-iterator */
    template <class Iterator>
    Object( Iterator begin, Iterator end )
    { for_( it, begin, end ) add( it->first, it->second ); }

    /** Construct from map-initializer list { {k1,v1}, {k2,v2},... } */
    Object( const std::initializer_list<std::pair<String, Value>> & contents_r );

    template <typename K, typename V>
    Object( std::multimap<K,V> values ) : _values( std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()) ) { }

    template <typename K, typename V>
    Object( std::map<K,V> values ) : _values( std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()) ) { }

    /** Add key/value pair */
    void add( String key_r, Value val_r );

    /** \overload from map-initializer list { {k1,v1}, {k2,v2},... } */
    void add(std::initializer_list<std::pair<String, Value> > contents_r );

    /** JSON representation */
    std::string asJSON() const;

    /** Stream output */
    std::ostream & dumpOn( std::ostream & str ) const;

    iterator begin() { return _values.begin (); }
    iterator end() { return _values.end (); }

    const_iterator begin() const { return _values.begin (); }
    const_iterator end() const { return _values.end (); }

    bool contains( const String& key ) const;

    size_type size() const {
      return _values.size ();
    }

    std::pair<iterator, iterator> equal_range( const String& key ) {
      return _values.equal_range (key);
    }

    std::pair<const_iterator, const_iterator> equal_range( const String& key ) const {
      return _values.equal_range (key);
    }

    std::vector<Value> values(const String &key) const;
    const Value &value( const String &key ) const;

    bool operator==( const Object &other ) const;

  private:
    std::ostream & dumpOn( std::ostream & str, std::map<String, Value>::const_iterator val_r ) const;
  private:
    std::multimap<String, Value> _values;
  };

  /** \relates Object Stream output */
  inline std::ostream & operator<<( std::ostream & str, const Object & obj )
  {
    return obj.dumpOn( str );
  }



  class Value {

  public:


    enum Type {
      NullType,
      BoolType,
      StringType,
      IntType,
      UIntType,
      NumberType,
      ObjectType,
      ArrayType
    };


    ~Value() = default;
    Value(const Value &) = default;
    Value(Value &&) = default;
    Value &operator=(const Value &) = default;
    Value &operator=(Value &&) = default;

    // null
    Value() = default;
    Value( std::nullptr_t ) { }

    // bool
    Value( bool val_r  ) : _value(Bool(val_r))     {}

    // numbers
    Value( std::int8_t val_r )  : _value(Int(val_r) ) {}
    Value( std::int16_t val_r ) : _value(Int(val_r) ) {}
    Value( std::int32_t val_r ) : _value(Int(val_r) ) {}
    Value( std::int64_t val_r ) : _value(Int(val_r) ) {}

    Value( std::uint8_t val_r )  : _value(UInt(val_r) ) {}
    Value( std::uint16_t val_r ) : _value(UInt(val_r) ) {}
    Value( std::uint32_t val_r ) : _value(UInt(val_r) ) {}
    Value( std::uint64_t val_r ) : _value(UInt(val_r) ) {}

    Value( std::float_t  val_r ) : _value(Number(val_r) ) {}
    Value( std::double_t val_r ) : _value(Number(val_r) ) {}

    // strings
    Value( const char val_r )    : _value(String(std::string( 1, val_r )) ) {}
    Value( const char * val_r )	 : _value(String(str::asString (val_r)) ) {}
    Value( std::string val_r )   : _value(String(std::move(val_r)) ) {}

    // Object
    //Value (const std::initializer_list<std::pair<String, Value>> & contents_r) : _value(Object(contents_r)) {}

    // Array
    // Value (const std::initializer_list<Value> &contents_r) : _value(Array(contents_r)) {}

    // JSON types
    Value( Null val_r  )    : _value( std::move(val_r) ) {}
    Value( Bool val_r  )    : _value( std::move(val_r) ) {}
    Value( String val_r  )  : _value( std::move(val_r) ) {}
    Value( Int val_r  )     : _value( std::move(val_r) ) {}
    Value( UInt val_r  )    : _value( std::move(val_r) ) {}
    Value( Number val_r  )  : _value( std::move(val_r) ) {}
    Value( Object val_r  )  : _value( std::move(val_r) ) {}
    Value( Array val_r  )   : _value( std::move(val_r) ) {}

    /** JSON representation */
    std::string asJSON() const;

    const Null &asNull() const {
      return std::get<Null>( _value );
    }

    const Bool &asBool() const {
      return std::get<Bool>( _value );
    }

    const String &asString() const {
      return std::get<String>( _value );
    }

    const Int &asInt() const {
      return std::get<Int>( _value );
    }

    const UInt &asUInt() const {
      return std::get<UInt>( _value );
    }

    const Number &asNumber() const {
      return std::get<Number>( _value );
    }

    const Object &asObject() const {
      return std::get<Object>( _value );
    }

    const Array &asArray() const {
      return std::get<Array>( _value );
    }

    /** Stream output */
    std::ostream & dumpOn( std::ostream & str ) const
    { return str << asJSON(); }

    bool isNull() const {
      return std::holds_alternative<Null>(_value);
    }

    Type type() const {
      return std::visit([](const auto &t) {
        using _Type = std::decay_t<decltype(t)>;
        if constexpr ( std::is_same_v<_Type, Bool> ) {
          return BoolType;
        } else if constexpr ( std::is_same_v<_Type, String> ) {
          return StringType;
        } else if constexpr ( std::is_same_v<_Type, Int> ) {
          return IntType;
        } else if constexpr ( std::is_same_v<_Type, UInt> ) {
          return UIntType;
        } else if constexpr ( std::is_same_v<_Type, Number> ) {
          return NumberType;
        } else if constexpr ( std::is_same_v<_Type, Object> ) {
          return ObjectType;
        } else if constexpr ( std::is_same_v<_Type, Array> ) {
          return ArrayType;
        }
        return NullType;
      }, _value );
    }

    // comparison
    bool operator==( const Value &other ) const { return _value == other._value; }

  private:
    using JsonValueData = std::variant<Null, Bool, String, Int, UInt, Number, Object, Array>;
    JsonValueData _value = Null();
  };

  template<class Iterator>
  Array::Array(Iterator begin, Iterator end)
  {
    std::for_each( begin, end, [this]( const auto &v) { this->add(v); } );
  }

  /** \relates Value Stream output */
  inline std::ostream & operator<<( std::ostream & str, const Value & obj )
  { return obj.dumpOn( str ); }

  template <typename T>
  Value toJSON( T &&v ) {
    return Value( std::forward<T>(v) );
  }
}

#endif
