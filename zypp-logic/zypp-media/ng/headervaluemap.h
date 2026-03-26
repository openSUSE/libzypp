/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_MEDIA_NG_HEADERVALUEMAP_H_INCLUDED
#define ZYPP_MEDIA_NG_HEADERVALUEMAP_H_INCLUDED

#include <variant>
#include <string>
#include <map>
#include <zypp-core/base/PtrTypes.h>

namespace zyppng {

  class HeaderValue
  {
  public:
    using value_type = std::variant<std::monostate, std::string, int32_t, int64_t, bool>;

    HeaderValue();

    HeaderValue( const HeaderValue &other );
    HeaderValue( HeaderValue &&other ) noexcept;

    HeaderValue( const bool val );
    HeaderValue( const int32_t val );
    HeaderValue( const int64_t val );
    HeaderValue( std::string &&val );
    HeaderValue( const std::string &val );
    HeaderValue( const char *val );

    bool valid () const;

    bool isString () const;
    bool isInt () const;
    bool isInt64 () const;
    bool isDouble () const;
    bool isBool () const;

    const std::string &asString () const;
    int32_t asInt   () const;
    int64_t asInt64 () const;
    bool asBool () const;

    value_type &asVariant ();
    const value_type &asVariant () const;

    HeaderValue &operator= ( const HeaderValue &other );
    HeaderValue &operator= ( HeaderValue &&other ) noexcept;
    HeaderValue &operator= ( const std::string &val );
    HeaderValue &operator= ( int32_t val );
    HeaderValue &operator= ( int64_t val );
    HeaderValue &operator= ( bool val );

    bool operator== ( const HeaderValue &other ) const;

  private:
    zypp::RWCOW_pointer<value_type> _val;
  };

  class HeaderValueMap
  {
  public:
    using Value = HeaderValue;
    using ValueMap = std::map<std::string, std::vector<Value>>;

    static Value InvalidValue;

    class const_iterator
    {
    public:
      using iterator_category = std::forward_iterator_tag;
      using value_type        = std::pair<std::string, Value>;
      using difference_type   = std::ptrdiff_t;
      using pointer           = void;
      using reference         = const std::pair<const std::string&, const Value&>;

    private:
      ValueMap::const_iterator _it;

    public:
      const_iterator() = default;

      explicit const_iterator( const ValueMap::const_iterator &val )
        : _it(val) {}

      const_iterator( const HeaderValueMap::const_iterator &other )
        : _it(other.base()) {}

      const std::string &key () const {
        return _it->first;
      }

      const Value &value() const {
        auto &l = _it->second;
        if ( l.empty() ) {
          return InvalidValue;
        }
        return l.back();
      }

      reference operator*() const {
        return reference( key(), value() );
      }

      const_iterator& operator++() {
        ++_it;
        return *this;
      }

      const_iterator operator++(int) {
        const_iterator tmp = *this;
        ++_it;
        return tmp;
      }

      bool operator==(const const_iterator& other) const {
        return _it == other._it;
      }

      bool operator!=(const const_iterator& other) const {
        return !(*this == other);
      }

      const ValueMap::const_iterator& base() const { return _it; }
    };

    HeaderValueMap() = default;
    HeaderValueMap( std::initializer_list<ValueMap::value_type> init );

    bool contains( const std::string &key ) const;
    bool contains( const std::string_view &key ) const {
      return contains(std::string(key));
    }

    void set( const std::string &key, Value val );
    void set( const std::string &key, std::vector<Value> val );
    void add( const std::string &key, const Value &val);
    void clear ();
    ValueMap::size_type size() const noexcept;

    std::vector<Value> &values ( const std::string &key );
    const std::vector<Value> &values ( const std::string &key ) const;

    std::vector<Value> &values ( const std::string_view &key ) {
      return values( std::string(key) );
    }

    const std::vector<Value> &values ( const std::string_view &key ) const {
      return values( std::string(key) );
    }

    /*!
     * Returns the last entry with key \a str in the list of values
     * or the default value specified in \a defaultVal
     */
    Value value ( const std::string_view &str, const Value &defaultVal = Value() ) const;
    Value value ( const std::string &str, const Value &defaultVal = Value() ) const;

    Value &operator[]( const std::string &key );
    Value &operator[]( const std::string_view &key );
    const Value &operator[]( const std::string &key ) const;
    const Value &operator[]( const std::string_view &key ) const;

    const_iterator erase( const const_iterator &i );
    bool erase( const std::string &key );

    const_iterator begin() const {
      return const_iterator( _values.begin() );
    }
    const_iterator end() const {
      return const_iterator( _values.end() );
    }

    ValueMap::iterator beginList() {
      return _values.begin();
    }
    ValueMap::iterator endList() {
      return _values.end();
    }

    ValueMap::const_iterator beginList() const {
      return _values.begin();
    }
    ValueMap::const_iterator endList() const {
      return _values.end();
    }

    ValueMap::const_iterator cbeginList() const {
      return _values.cbegin();
    }
    ValueMap::const_iterator cendList() const {
      return _values.cend();
    }

  private:
    ValueMap _values;
  };
}

namespace zypp {
  template<>
  inline zyppng::HeaderValue::value_type* rwcowClone<zyppng::HeaderValue::value_type>( const zyppng::HeaderValue::value_type * rhs )
  { return new zyppng::HeaderValue::value_type(*rhs); }
}


#endif
