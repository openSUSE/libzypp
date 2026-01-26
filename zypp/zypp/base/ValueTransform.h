/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/ValueTransform.h
 */
#ifndef ZYPP_BASE_VALUETRANSFORM_H
#define ZYPP_BASE_VALUETRANSFORM_H

#include <iosfwd>

#include <zypp-core/base/Iterator.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace base
  {
    ///////////////////////////////////////////////////////////////////
    /// \class ValueTransform
    /// \brief Helper managing raw values with transformed representation
    ///
    /// This helper enforces to explicitly state whether you are using
    /// the raw or the variable replaced value. Usually you set \c raw
    /// and get \c transformed (unless writing \c raw to some config file).
    ///
    /// Used e.g. vor variable replaced config strings.
    ///////////////////////////////////////////////////////////////////
    template<class Tp, class TUnaryFunction>
    struct ValueTransform
    {
      using RawType = Tp;
      using Transformator = TUnaryFunction;
#if __cplusplus < 201703L
      using TransformedType = std::result_of_t<Transformator (RawType)>; // yast in 15.[23] uses c++11
#else
      using TransformedType = std::invoke_result_t<Transformator, RawType>;
#endif

    public:
      ValueTransform()
      {}

      explicit ValueTransform( RawType raw_r )
      : _raw( std::move(raw_r) )
      {}

      ValueTransform( RawType raw_r, Transformator transform_r )
      : _raw( std::move(raw_r) ), _transform( std::move(transform_r) )
      {}

    public:
      /** Get the raw value */
      const RawType & raw() const
      { return _raw; }

      /** Set the raw value */
      RawType & raw()
      { return _raw; }

    public:
      /** Return a transformed copy of the raw value */
      TransformedType transformed() const
      { return _transform( _raw ); }

      /** Return a transformed copy of an arbitrary \a RawType */
      TransformedType transformed( const RawType & raw_r ) const
      { return _transform( raw_r ); }

      /** Return the transformator */
      const Transformator & transformator() const
      { return _transform; }

    private:
      RawType _raw;
      Transformator _transform;
    };

    ///////////////////////////////////////////////////////////////////
    /// \class ContainerTransform
    /// \brief Helper managing a container of raw values with transformed representation
    ///
    /// This helper enforces to explicitly state wheter you are using
    /// the raw or the variable replaced value. Usually you set \c raw
    /// and get \c transformed (uness writing \c raw to some config file).
    ///
    /// Offers iterating over transformed strings in the list.
    ///////////////////////////////////////////////////////////////////
    template<class TContainer, class TUnaryFunction>
    struct ContainerTransform
    {
      using Container = TContainer;
      using Transformator = TUnaryFunction;
      using size_type = typename Container::size_type;
      using RawType = typename Container::value_type;
#if __cplusplus < 201703L
      using TransformedType = std::result_of_t<Transformator (RawType)>; // yast in 15.[23] uses c++11
#else
      using TransformedType = std::invoke_result_t<Transformator, RawType>;
#endif
    public:
      ContainerTransform()
      {}

      explicit ContainerTransform( Container raw_r )
      : _raw( std::move(raw_r) )
      {}

      ContainerTransform( Container raw_r, Transformator transform_r )
      : _raw( std::move(raw_r) ), _transform( std::move(transform_r) )
      {}

    public:
      bool empty() const
      { return _raw.empty(); }

      size_type size() const
      { return _raw.size(); }

      using RawConstIterator = typename Container::const_iterator;

      RawConstIterator rawBegin() const
      { return _raw.begin(); }

      RawConstIterator rawEnd() const
      { return _raw.end(); }

      /** Get the raw value */
      const Container & raw() const
      { return _raw; }

      /** Set the raw value */
      Container & raw()
      { return _raw; }

      /** Clear the container */
      void clear()
      { _raw.clear(); }

    public:
      using TransformedConstIterator = transform_iterator<Transformator, typename Container::const_iterator>;

      TransformedConstIterator transformedBegin() const
      { return make_transform_iterator( _raw.begin(), _transform ); }

      TransformedConstIterator transformedEnd() const
      { return make_transform_iterator( _raw.end(), _transform ); }

      /** Return copy with transformed variables (expensive) */
      Container transformed() const
      { return Container( transformedBegin(), transformedEnd() ); }

      /** Return a transformed copy of an arbitrary \a RawType */
      TransformedType transformed( const RawType & raw_r ) const
      { return _transform( raw_r ); }

      /** Return a transformed copy of a \a RawConstIterator raw value */
      TransformedType transformed( const RawConstIterator & rawIter_r ) const
      { return _transform( *rawIter_r ); }

      /** Return the transformator */
      const Transformator & transformator() const
      { return _transform; }

    private:
      Container _raw;
      Transformator _transform;
    };

  } // namespace base
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_BASE_VALUETRANSFORM_H
