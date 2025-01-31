/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_CONFIGVALUES_H
#define ZYPP_CONFIGVALUES_H

#include <iosfwd>
#include <map>
#include <string_view>

#include <zypp/base/StringV.h>

///////////////////////////////////////////////////////////////////
namespace zypp {

  class ConfigValueNode
  {
  public:
    using KeyType    = std::string_view;
    using ValueType  = std::string;
    using OptionType = std::optinal<ValueType>;

  public:
    void set( const KeyType & key_r, ValueType value_r );
    void unset( const KeyType & key_r );

    /** get */
    const OptionType & get( const KeyType & key_r ) const;

    /** get converted */
    template<typename Tp>
    std::optinal<Tp> getAs( const KeyType & key_r ) const;

    /** get converted and assign if not nollopt */
    template<typename Tp>
    std::optinal<Tp> getAs( const KeyType & key_r, Tp & store_r ) const;

  private:
    std::map<KeyType,OptionType>      _values;
    std::map<KeyType,ConfigValueNode> _subnodes;
  };

  class ConfigValues
  {
    using KeyType    = ConfigValueNode::KeyType;
    using ValueType  = ConfigValueNode::ValueType;
    using OptionType = ConfigValueNode::OptionType;

  public:
    ConfigValues() : _root { new ConfigValueNode } {}

    // conveniencce forwarding to _root

  private:
    RW_pointer<ConfigValueNode> _root;  // reference semantic
  };

} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_CONFIGVALUES_H
