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
#include <vector>
#include <optional>
#include <string>
#include <string_view>

#include <zypp/base/Exception.h>

///////////////////////////////////////////////////////////////////
namespace zypp {

  ///////////////////////////////////////////////////////////////////
  struct ConfigValueException : public Exception
  { ConfigValueException( std::string msg ) : Exception( std::move(msg) ) {} };

  ///////////////////////////////////////////////////////////////////
  /// \class ConfigValue
  /// \brief A string based config value remembering an initial default.
  class ConfigValue
  {
  public:
    using ValueType = std::optional<std::string>;
    using ValueView = std::optional<std::string_view>;

  public:
    ConfigValue();
    ConfigValue( ValueType default_r );
    ~ConfigValue();

  public:
    ValueView value() const;
    void setValue( ValueType value_r );

    bool usesDefault() const;
    void resetToDefault();

  public:
    bool hasDefaultValue() const;
    ValueView defaultValue() const;
    void setDefaultValue( ValueType value_r );

  private:
    std::optional<std::string> _value;
    std::optional<std::string> _default;
  };


  ///////////////////////////////////////////////////////////////////
  /// \class ConfigValues
  /// \brief A tree of \ref ConfigValue nodes.
  ///
  /// A node in the tree consists of:
  ///
  /// \b label: a string identifying the node in a tree path
  ///           (must not contain \c /, \c *, or \c [)
  ///
  /// \b value: the \ref ConfigValue
  ///
  /// A node is addressed by a path like expression.
  ///
  /// \code
  /// NODES:      Paths:
  /// +group      /group
  ///      +a     /group/a[1]
  ///      +a     /group/a[2]
  ///      +b     /group/b
  ///      +c     /group/c
  ///      +a     /group/a[3]
  /// \endcode
  class ConfigValues
  {
  public:
    using ExceptionType = ConfigValueException;
    using LabelType     = std::string;
    using PathType      = std::string_view;
    using ValueType     = ConfigValue::ValueType;
    //using ValueView     = ConfigValue::ValueView;

  public:
    ConfigValues();
    ~ConfigValues();

  public:
    /** Whether \a path_r denotes at least one node. */
    bool hasNode( PathType path_r ) const;

    /** Number of nodes matching \a path_r. */
    unsigned countNode( PathType path_r ) const;

    /** Find a unique node matching \a path_r if it exists.
     *
     * In case multiple sibling nodes with the same label exist,
     * \a which_r denotes the one to be returned. \c 1 for the first,
     * \c 2 for the second, etc. If \a which_r is greater than the number
     * of sibling nodes with the same label, the last one is returned.
     * The default \c 0 expects the node to be unique.
     *
     * \throws ConfigValueException
     * If \a path_r denotes more than one node.
     */
    std::optional<const ConfigValue> findNode( PathType path_r, unsigned which_r = 0 ) const;
    std::optional<ConfigValue>       findNode( PathType path_r, unsigned which_r = 0 );

    /** Find all nodes matching \a path_r. */
    std::vector<const ConfigValue> findNodes( PathType path_r ) const;
    std::vector<ConfigValue>       findNodes( PathType path_r );

    /** Get the unique node matching \a path_r (const).
     *
     * \throws ConfigValueException
     * If \a path_r denotes no or more than one node.
     */
    const ConfigValue & getNode( PathType path_r, unsigned which_r = 0 ) const;

    /** Get or create the unique node matching \a path_r (not const).
     *
     * If the node does not exits, it is created.
     *
     * In case multiple sibling nodes with the same label exist,
     * \a which_r denotes the one to be returned. \c 1 for the first,
     * \c 2 for the second, etc. If \a which_r is greater than the number
     * of sibling nodes with the same label, a new one will be created at the end.
     * The default \c 0 expects the node to be unique.
     *
     * \throws ConfigValueException
     * If \a path_r denotes more than one node.
     */
    ConfigValue & getNode( PathType path_r, unsigned which_r = 0 );

    /** Get or create the unique node matching \a path_r and assign \a value_r to the node.
     *
     * If the node is newly created, \a value_r will also become the
     * nodes default value.
     *
     * \throws ConfigValueException
     * If \a path_r denotes more than one node.
     */
    ConfigValue & setNode( PathType path_r, ValueType value_r, unsigned which_r = 0 );

#if 0
    /** Insert a new node with label \a label_r before or after a \ref sibling_r.
     *
     * \throws ConfigValueException
     * If \a sibling_r denotes more than one node.
     */
    ConfigValue & insertNode( PathType sibling_r, LabelType label_r, bool before_r = true ); // *
    ConfigValue & insertNode( PathType sibling_r, LabelType label_r, ValueType value_r, bool before = true ); // *
#endif

    /** Remove all nodes matching \ref path_r (and their children).
     * \return the number of removed nodes.
     */
    unsigned removeNode( PathType path_r );

  public:
    class Impl;
  private:
    RW_pointer<Impl> _pimpl;  // reference semantic
  };


  /** \relates ConfigValues Stream output */
  std::ostream & operator<<( std::ostream & str, const ConfigValues & obj );

} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_CONFIGVALUES_H
