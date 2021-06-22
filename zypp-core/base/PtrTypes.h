/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/base/PtrTypes.h
 *  \ingroup ZYPP_SMART_PTR
 *  \see ZYPP_SMART_PTR
*/
#ifndef ZYPP_BASE_PTRTYPES_H
#define ZYPP_BASE_PTRTYPES_H

#include <iosfwd>
#include <string>

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/intrusive_ptr.hpp>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  namespace str
  {
    // printing void* (prevents us from including <ostream>)
    std::string form( const char * format, ... ) __attribute__ ((format (printf, 1, 2)));
  }

    /** \defgroup ZYPP_SMART_PTR Smart pointer types
     *  Smart pointer types.
     *
     * Namespace zypp provides 3 smart pointer types \b using the
     * boost smart pointer library.
     *
     * \li \c scoped_ptr Simple sole ownership of single objects. Noncopyable.
     *
     * \li \c shared_ptr Object ownership shared among multiple pointers
     *
     * \li \c weak_ptr Non-owning observers of an object owned by shared_ptr.
     *
     * And \ref zypp::RW_pointer, as wrapper around a smart pointer,
     * poviding \c const correct read/write access to the object it refers.
    */
    /*@{*/

    /** shared_ptr custom deleter doing nothing.
     * A custom deleter is a function being called when the
     * last shared_ptr goes out of score. Per default the
     * object gets deleted, but you can insall custom deleters
     * as well. This one does nothing.
     *
     * \code
     *  // Some class providing a std::istream
     *  struct InpuStream
     * {
     *   // Per deafult use std::cin.
     *   InputStream()
     *   : _stream( &std::cin, NullDeleter() )
     *   {}
     *   // Or read from a file.
     *   InputStream( const Pathname & file_r )
     *   : _stream( new ifgzstream( _path.asString().c_str() ) )
     *   {}
     *   // Or use a stream priovided by the application.
     *   InputStream( std::istream & stream_r )
     *   : _stream( &stream_r, NullDeleter() )
     *   {}
     *
     *   std::istream & stream()
     *   { return *_stream; }
     *
     * private:
     *   shared_ptr<std::istream> _stream;
     * };
     * \endcode
    */
    struct NullDeleter
    {
      void operator()( const void *const ) const
      {}
    };

    /** \class scoped_ptr */
    using boost::scoped_ptr;

    /** \class shared_ptr */
    using boost::shared_ptr;

    /** \class weak_ptr */
    using boost::weak_ptr;

    /** \class intrusive_ptr */
    using boost::intrusive_ptr;

    /** */
    using boost::static_pointer_cast;
    /**  */
    using boost::const_pointer_cast;
    /**  */
    using boost::dynamic_pointer_cast;

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
namespace std
{ /////////////////////////////////////////////////////////////////

  // namespace sub {
  //    class Foo;
  //    typedef zypp::intrusive_ptr<Foo> Foo_Ptr; // see DEFINE_PTR_TYPE(NAME) macro below
  // }

  // Defined in namespace std g++ finds the output operator (König-Lookup),
  // even if we typedef the pointer in a different namespace than ::zypp.
  // Otherwise we had to define an output operator always in the same namespace
  // as the typedef (else g++ will just print the pointer value).

  /** \relates zypp::shared_ptr Stream output. */
  template<class D>
  inline std::ostream & operator<<( std::ostream & str, const zypp::shared_ptr<D> & obj )
  {
    if ( obj )
      return str << *obj;
    return str << std::string("NULL");
  }
  /** \overload specialize for void */
  template<>
  inline std::ostream & operator<<( std::ostream & str, const zypp::shared_ptr<void> & obj )
  {
    if ( obj )
      return str << zypp::str::form( "%p", (void*)obj.get() );
    return str << std::string("NULL");
  }

  /** \relates zypp::shared_ptr Stream output. */
  template<class D>
  inline std::ostream & dumpOn( std::ostream & str, const zypp::shared_ptr<D> & obj )
  {
    if ( obj )
      return dumpOn( str, *obj );
    return str << std::string("NULL");
  }
  /** \overload specialize for void */
  template<>
  inline std::ostream & dumpOn( std::ostream & str, const zypp::shared_ptr<void> & obj )
  { return str << obj; }

  /** \relates zypp::intrusive_ptr Stream output. */
  template<class D>
  inline std::ostream & operator<<( std::ostream & str, const zypp::intrusive_ptr<D> & obj )
  {
    if ( obj )
      return str << *obj;
    return str << std::string("NULL");
  }
  /** \relates zypp::intrusive_ptr Stream output. */
  template<class D>
  inline std::ostream & dumpOn( std::ostream & str, const zypp::intrusive_ptr<D> & obj )
  {
    if ( obj )
      return dumpOn( str, *obj );
    return str << std::string("NULL");
  }
  /////////////////////////////////////////////////////////////////
} // namespace std
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	RW_pointer traits
    //
    ///////////////////////////////////////////////////////////////////
    /**
     * Don't forgett to provide versions for PtrType and constPtrType,
     * esp. if creation a of temporary is not acceptable (eg. when
     * checking the ref count value).
    */
    namespace rw_pointer {

      template<class D>
        struct Shared
        {
          typedef shared_ptr<D>       PtrType;
          typedef shared_ptr<const D> constPtrType;
          /** Check whether pointer is not shared. */
          bool unique( const constPtrType & ptr_r )
          { return !ptr_r || ptr_r.unique(); }
          bool unique( const PtrType & ptr_r )
          { return !ptr_r || ptr_r.unique(); }
          /** Return number of references. */
          long use_count( const constPtrType & ptr_r ) const
          { return ptr_r.use_count(); }
          long use_count( const PtrType & ptr_r ) const
          { return ptr_r.use_count(); }
        };

      template<class D>
        struct Intrusive
        {
          typedef intrusive_ptr<D>       PtrType;
          typedef intrusive_ptr<const D> constPtrType;
          /** Check whether pointer is not shared. */
          bool unique( const constPtrType & ptr_r )
          { return !ptr_r || (ptr_r->refCount() <= 1); }
          bool unique( const PtrType & ptr_r )
          { return !ptr_r || (ptr_r->refCount() <= 1); }
          /** Return number of references. */
          long use_count( const constPtrType & ptr_r ) const
          { return ptr_r ? ptr_r->refCount() : 0; }
          long use_count( const PtrType & ptr_r ) const
          { return ptr_r ? ptr_r->refCount() : 0; }
        };

       template<class D>
        struct Scoped
        {
          typedef scoped_ptr<D>       PtrType;
          typedef scoped_ptr<const D> constPtrType;
          /** Check whether pointer is not shared. */
          bool unique( const constPtrType & ptr_r )
          { return true; }
          bool unique( const PtrType & ptr_r )
          { return true; }
          /** Return number of references. */
          long use_count( const constPtrType & ptr_r ) const
          { return ptr_r ? 1 : 0; }
          long use_count( const PtrType & ptr_r ) const
          { return ptr_r ? 1 : 0; }
        };

   }
    ///////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : RW_pointer
    //
    /** Wrapper for \c const correct access via \ref ZYPP_SMART_PTR.
     *
     * zypp::RW_pointer<tt>\<D,DTraits></tt> stores a \ref ZYPP_SMART_PTR
     * of type \c DTraits::PtrType, which must be convertible into a <tt>D *</tt>.
     * Pointer style access (via \c -> and \c *) offers a <tt>const D *</tt> in const
     * a context, otherwise a <tt>D *</tt>. Thus \em RW_ means \em read/write,
     * as you get a different type, dependent on whether you're allowed to
     * read or write.
     *
     * Forwarding access from an interface to an implemantation class, an
     * RW_pointer prevents const interface methods from accidentally calling
     * nonconst implementation methods.
     *
     * The second template argument defaults to
     * <tt>DTraits = rw_pointer::Shared<D></tt> thus wraping a
     * <tt>shared_ptr<D></tt>. To wrap an <tt>intrusive_ptr<D></tt>
     * use <tt>rw_pointer::Intrusive<D></tt>.
     *
     * \see zypp::RWCOW_pointer for 'copy on write' functionality.
     *
     * \code
     * #include "zypp/base/PtrTypes.h"
     *
     * class Foo
     * {
     *   ...
     *   private:
     *     // Implementation class
     *     struct Impl;
     *     // Pointer to implementation; actually a shared_ptr<Impl>
     *     RW_pointer<Impl> _pimpl;
     *
     *     void baa()       { _pimpl->... } // is Impl *
     *     void baa() const { _pimpl->... } // is Impl const *
     * };
     * \endcode
    */
    template<class D, class DTraits = rw_pointer::Shared<D> >
      struct RW_pointer
      {
        typedef typename DTraits::PtrType               PtrType;
        typedef typename DTraits::constPtrType          constPtrType;

        RW_pointer()
        {}

        RW_pointer( std::nullptr_t )
        {}

        explicit
        RW_pointer( typename PtrType::element_type * dptr )
        : _dptr( dptr )
        {}

        explicit
        RW_pointer( PtrType dptr )
        : _dptr( dptr )
        {}

        RW_pointer & operator=( std::nullptr_t )
	{ reset(); return *this; }

        void reset()
        { PtrType().swap( _dptr ); }

        void reset( typename PtrType::element_type * dptr )
        { PtrType( dptr ).swap( _dptr ); }

        void swap( RW_pointer & rhs )
        { _dptr.swap( rhs._dptr ); }

        void swap( PtrType & rhs )
        { _dptr.swap( rhs ); }

        explicit operator bool() const
        { return _dptr.get() != nullptr; }

        const D & operator*() const
        { return *_dptr; };

        const D * operator->() const
        { return _dptr.operator->(); }

        const D * get() const
        { return _dptr.get(); }

        D & operator*()
        { return *_dptr; }

        D * operator->()
        { return _dptr.operator->(); }

        D * get()
        { return _dptr.get(); }

      public:
        bool unique() const
	{ return DTraits().unique( _dptr ); }

	long use_count() const
	{ return DTraits().use_count( _dptr ); }

        constPtrType getPtr() const
        { return _dptr; }

        PtrType getPtr()
        { return _dptr; }

        constPtrType cgetPtr()
        { return _dptr; }

      private:
        PtrType _dptr;
      };
    ///////////////////////////////////////////////////////////////////

    /** \relates RW_pointer Stream output.
     *
     * Print the \c D object the RW_pointer refers, or \c "NULL"
     * if the pointer is \c NULL.
     */
    template<class D, class DPtr>
      inline std::ostream & operator<<( std::ostream & str, const RW_pointer<D, DPtr> & obj )
      {
        if ( obj.get() )
          return str << *obj.get();
        return str << std::string("NULL");
      }

    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const RW_pointer<D, DPtr> & lhs, const RW_pointer<D, DPtr> & rhs )
      { return( lhs.get() == rhs.get() ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const RW_pointer<D, DPtr> & lhs, const typename DPtr::PtrType & rhs )
      { return( lhs.get() == rhs.get() ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const typename DPtr::PtrType & lhs, const RW_pointer<D, DPtr> & rhs )
      { return( lhs.get() == rhs.get() ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const RW_pointer<D, DPtr> & lhs, const typename DPtr::constPtrType & rhs )
      { return( lhs.get() == rhs.get() ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const typename DPtr::constPtrType & lhs, const RW_pointer<D, DPtr> & rhs )
      { return( lhs.get() == rhs.get() ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const RW_pointer<D, DPtr> & lhs, std::nullptr_t )
      { return( lhs.get() == nullptr ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator==( std::nullptr_t, const RW_pointer<D, DPtr> & rhs )
      { return( nullptr == rhs.get() ); }


    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const RW_pointer<D, DPtr> & lhs, const RW_pointer<D, DPtr> & rhs )
      { return ! ( lhs == rhs ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const RW_pointer<D, DPtr> & lhs, const typename DPtr::PtrType & rhs )
      { return ! ( lhs == rhs ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const typename DPtr::PtrType & lhs, const RW_pointer<D, DPtr> & rhs )
      { return ! ( lhs == rhs ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const RW_pointer<D, DPtr> & lhs, const typename DPtr::constPtrType & rhs )
      { return ! ( lhs == rhs ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const typename DPtr::constPtrType & lhs, const RW_pointer<D, DPtr> & rhs )
      { return ! ( lhs == rhs ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const RW_pointer<D, DPtr> & lhs, std::nullptr_t )
      { return( lhs.get() != nullptr ); }
    /** \relates RW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( std::nullptr_t, const RW_pointer<D, DPtr> & rhs )
      { return( nullptr != rhs.get() ); }

    ///////////////////////////////////////////////////////////////////

    /** \relates RWCOW_pointer Clone the underlying object.
     * Calls \a rhs <tt>-\>clone()</tt>. Being defined as a
     * function outside \ref RWCOW_pointer allows to overload
     * it, in case a specific \a D does not have <tt>clone()</tt>.
     */
    template<class D>
      inline D * rwcowClone( const D * rhs )
      { return rhs->clone(); }

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : RWCOW_pointer
    //
    /** \ref RW_pointer supporting 'copy on write' functionality.
     *
     * \em Write access to the underlying object creates a copy, iff
     * the object is shared.
     *
     * See \ref RW_pointer.
    */
    template<class D, class DTraits = rw_pointer::Shared<D> >
      struct RWCOW_pointer
      {
        typedef typename DTraits::PtrType               PtrType;
        typedef typename DTraits::constPtrType          constPtrType;

	RWCOW_pointer()
	{}

	RWCOW_pointer( std::nullptr_t )
	{}

        explicit
        RWCOW_pointer( typename PtrType::element_type * dptr )
        : _dptr( dptr )
        {}

        explicit
        RWCOW_pointer( PtrType dptr )
        : _dptr( dptr )
        {}

        RWCOW_pointer & operator=( std::nullptr_t )
	{ reset(); return *this; }

        void reset()
        { PtrType().swap( _dptr ); }

        void reset( typename PtrType::element_type * dptr )
        { PtrType( dptr ).swap( _dptr ); }

        void swap( RWCOW_pointer & rhs )
        { _dptr.swap( rhs._dptr ); }

        void swap( PtrType & rhs )
        { _dptr.swap( rhs ); }

        explicit operator bool() const
	{ return _dptr.get() != nullptr; }

        const D & operator*() const
        { return *_dptr; };

        const D * operator->() const
        { return _dptr.operator->(); }

        const D * get() const
        { return _dptr.get(); }

        D & operator*()
        { assertUnshared(); return *_dptr; }

        D * operator->()
        { assertUnshared(); return _dptr.operator->(); }

        D * get()
        { assertUnshared(); return _dptr.get(); }

      public:
        bool unique() const
	{ return DTraits().unique( _dptr ); }

	long use_count() const
	{ return DTraits().use_count( _dptr ); }

        constPtrType getPtr() const
        { return _dptr; }

        PtrType getPtr()
        { assertUnshared(); return _dptr; }

        constPtrType cgetPtr()
        { return _dptr; }

      private:

        void assertUnshared()
        {
          if ( !unique() )
            PtrType( rwcowClone( _dptr.get() ) ).swap( _dptr );
        }

      private:
        PtrType _dptr;
      };
    ///////////////////////////////////////////////////////////////////

    /** \relates RWCOW_pointer Stream output.
     *
     * Print the \c D object the RWCOW_pointer refers, or \c "NULL"
     * if the pointer is \c NULL.
     */
    template<class D, class DPtr>
      inline std::ostream & operator<<( std::ostream & str, const RWCOW_pointer<D, DPtr> & obj )
      {
        if ( obj.get() )
          return str << *obj.get();
        return str << std::string("NULL");
      }

    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const RWCOW_pointer<D, DPtr> & lhs, const RWCOW_pointer<D, DPtr> & rhs )
      { return( lhs.get() == rhs.get() ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const RWCOW_pointer<D, DPtr> & lhs, const typename DPtr::PtrType & rhs )
      { return( lhs.get() == rhs.get() ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const typename DPtr::PtrType & lhs, const RWCOW_pointer<D, DPtr> & rhs )
      { return( lhs.get() == rhs.get() ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const RWCOW_pointer<D, DPtr> & lhs, const typename DPtr::constPtrType & rhs )
      { return( lhs.get() == rhs.get() ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const typename DPtr::constPtrType & lhs, const RWCOW_pointer<D, DPtr> & rhs )
      { return( lhs.get() == rhs.get() ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator==( const RWCOW_pointer<D, DPtr> & lhs, std::nullptr_t )
      { return( lhs.get() == nullptr ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator==( std::nullptr_t, const RWCOW_pointer<D, DPtr> & rhs )
      { return( nullptr == rhs.get() ); }

    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const RWCOW_pointer<D, DPtr> & lhs, const RWCOW_pointer<D, DPtr> & rhs )
      { return ! ( lhs == rhs ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const RWCOW_pointer<D, DPtr> & lhs, const typename DPtr::PtrType & rhs )
      { return ! ( lhs == rhs ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const typename DPtr::PtrType & lhs, const RWCOW_pointer<D, DPtr> & rhs )
      { return ! ( lhs == rhs ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const RWCOW_pointer<D, DPtr> & lhs, const typename DPtr::constPtrType & rhs )
      { return ! ( lhs == rhs ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const typename DPtr::constPtrType & lhs, const RWCOW_pointer<D, DPtr> & rhs )
      { return ! ( lhs == rhs ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( const RWCOW_pointer<D, DPtr> & lhs, std::nullptr_t )
      { return( lhs.get() != nullptr ); }
    /** \relates RWCOW_pointer */
    template<class D, class DPtr>
      inline bool operator!=( std::nullptr_t, const RWCOW_pointer<D, DPtr> & rhs )
      { return( nullptr != rhs.get() ); }

    ///////////////////////////////////////////////////////////////////

    /*@}*/
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////

/** Forward declaration of Ptr types */
#define DEFINE_PTR_TYPE(NAME) \
class NAME;                                                      \
extern void intrusive_ptr_add_ref( const NAME * );               \
extern void intrusive_ptr_release( const NAME * );               \
typedef zypp::intrusive_ptr<NAME>       NAME##_Ptr;        \
typedef zypp::intrusive_ptr<const NAME> NAME##_constPtr;

///////////////////////////////////////////////////////////////////
#endif // ZYPP_BASE_PTRTYPES_H
