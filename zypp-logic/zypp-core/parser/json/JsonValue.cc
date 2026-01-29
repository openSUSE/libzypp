/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "JsonValue.h"

namespace zypp::json {

  Array::Array() {}

  Array::Array( std::initializer_list<Value> contents_r )
    : Array( contents_r.begin(), contents_r.end() )
  {}

  void Array::add(Value val_r)
  { _values.push_back( std::move(val_r) ); }

  void Array::add(const std::initializer_list<Value> &contents_r)
  {
    for( const auto &v : contents_r )
      add( v );
  }

  std::ostream &Array::dumpOn(std::ostream &str) const
  {
    if ( _values.empty() )
      return str << "[]";
    str << '[' << _values.begin()->asJSON();
    for( auto val = ++_values.begin(); val != _values.end(); val++ )
      str << ", " << val->asJSON();
    return str << ']';
  }

  const Value &Array::operator[](size_type n) const
  {
    return _values[n];
  }

  Array::size_type Array::size() const { return _values.size(); }

  Value &Array::operator[](size_type n)
  {
    return _values[n];
  }

  bool Array::operator==(const Array &other) const
  {
    return (other._values == _values);
  }

  Object::Object() {}

  Object::Object(const std::initializer_list<std::pair<String, Value> > &contents_r)
    : Object( contents_r.begin(), contents_r.end() )
  {}

  void Object::add(String key_r, Value val_r )
  { _values.insert( std::make_pair( std::move(key_r), std::move(val_r)) ); }

  void Object::add(std::initializer_list<std::pair<String, Value> > contents_r)
  {
    for( const auto &val : contents_r )
      add( val.first, val.second );
  }

  std::string Object::asJSON() const
  { return str::Str() << *this; }

  std::ostream &Object::dumpOn(std::ostream &str) const
  {
    using std::endl;
    if ( _values.empty() )
      return str << "{}";
    dumpOn( str << '{' << endl, _values.begin() );
    for_ ( val, ++_values.begin(), _values.end() )
        dumpOn( str << ',' << endl, val );
    return str << endl << '}';
  }

  bool Object::contains(const String &key) const {
    return ( _values.find(key) != _values.end() );
  }

  std::vector<Value> Object::values(const String &key) const
  {
    std::vector<Value> vs;
    auto range = _values.equal_range ( key );
    std::for_each( range.first, range.second, [&]( const auto &v) { vs.push_back (v.second);} );
    return vs;
  }

  const Value &Object::value(const String &key) const
  {
    auto i = _values.find (key);
    return i->second;
  }

  bool Object::operator==(const Object &other) const {
    return (_values == other._values);
  }

  std::ostream &Object::dumpOn(std::ostream &str, std::map<String, Value>::const_iterator val_r) const
  { return str << val_r->first << ": " << val_r->second; }

  std::string Value::asJSON() const
  {
    return std::visit( []( auto &&val ) { return val.asJSON(); }, _value );
  }
} // namespace zypp::json
