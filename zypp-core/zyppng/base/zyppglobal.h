#ifndef ZYPP_NG_BASE_ZYPPGLOBAL_H_INCLUDED
#define ZYPP_NG_BASE_ZYPPGLOBAL_H_INCLUDED

#include <memory>
#include <zypp-core/base/Easy.h>

#ifndef EXPORT_EXPERIMENTAL_API
#define LIBZYPP_NG_EXPORT
#define LIBZYPP_NG_NO_EXPORT
#else
#include <zypp-ng_export.h>
#endif

/*
 * Convenience helpers to automatically generate boilerplate code
 * for pimpl classes.
 *
 * Libzypp is using the PIMPL pattern to ensure binary compatiblity between
 * different version releases. This keeps rebuilds of applications
 * that link against libzypp to a minimum. A PIMPL class simply hides the
 * data members and functions that are not part of the public API/ABI in a
 * hidden private class, that is only accessible in the implementation files.
 * This allows even bigger refactorings to happen behind the scenes.
 *
 * A simple example would be:
 *
 * \code
 *
 * // MyClass.h
 *
 * // forward declare the private class, always use the public classname
 * // with a "Private" postfix:
 * class MyClassPrivate;
 *
 * class MyClass
 * {
 * public:
 *   // add all public API functions here
 *   void doSomething();
 *   int  getSomething() const;
 * private:
 *   // generate the forward declarations for the pimpl access functions
 *   ZYPP_DECLARE_PRIVATE(MyClass)
 *   // the only data member in the public class should be a pointer to the private type
 *   // named d_ptr
 *   std::unique_ptr<MyClassPrivate> d_ptr;
 * };
 *
 * // MyClass.cc
 *
 * // in the implementation file we can now define the private class:
 * class MyClassPrivate
 * {
 * public:
 *   // add the data members and private functions here
 *   int something = 0;
 * };
 *
 * // in the constructor make sure that the private part of the class
 * // is initialized too
 * MyClass::MyClass() : d_ptr( new MyClassPrivate )
 * {}
 *
 * int MyClass::getSomething() const
 * {
 *   // automatically generates a pointer named "d" to the
 *   // pimpl object
 *   Z_D();
 *   return d->something;
 * }
 *
 * void MyClass::doSomething()
 * {
 *   // It is also possible to use the d_func() to access the pointer:
 *   d_func()->something = 10;
 * }
 *
 * \endcode
 *
 * \note those macros are inspired by the Qt framework
 */

template <typename T> inline T *zyppGetPtrHelper(T *ptr) { return ptr; }
template <typename Ptr> inline auto zyppGetPtrHelper(const Ptr &ptr) -> decltype(ptr.operator->()) { return ptr.operator->(); }
template <typename Ptr> inline auto zyppGetPtrHelper(Ptr &ptr) -> decltype(ptr.operator->()) { return ptr.operator->(); }

#define ZYPP_DECLARE_PRIVATE(Class) \
    Class##Private* d_func();\
    const Class##Private* d_func() const; \
    friend class Class##Private;

#define ZYPP_IMPL_PRIVATE(Class) \
    Class##Private* Class::d_func() \
    { return static_cast<Class##Private *>(zyppGetPtrHelper(d_ptr)); } \
    const Class##Private* Class::d_func() const \
    { return static_cast<const Class##Private *>(zyppGetPtrHelper(d_ptr)); }

#define ZYPP_DECLARE_PUBLIC(Class)            \
    public:                                            \
    inline Class* z_func() { return static_cast<Class *>(z_ptr); } \
    inline const Class* z_func() const { return static_cast<const Class *>(z_ptr); } \
    friend class Class; \
    private:

#define Z_D() auto const d = d_func()
#define Z_Z() auto const z = z_func()

namespace zyppng {
  template <typename T>
  using Ref = std::shared_ptr<T>;

  template <typename T>
  using WeakRef = std::weak_ptr<T>;
}

/*!
 * Helper macro to declare Ref types
 */
#define ZYPP_FWD_DECL_REFS(T) \
  using T##Ref = ::zyppng::Ref<T>; \
  using T##WeakRef = ::zyppng::WeakRef<T>

/*
 * Helper Macro to forward declare types and ref types
 */
#define ZYPP_FWD_DECL_TYPE_WITH_REFS(T) \
  class T; \
  ZYPP_FWD_DECL_REFS(T)

#define ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS_ARG1(T, TArg1) \
  template< typename TArg1> \
  class T; \
  template< typename TArg1> \
  using T##Ref = Ref<T<TArg1>>; \
  template< typename TArg1> \
  using T##WeakRef = WeakRef<T<TArg1>>


//@TODO enable for c++20
#if 0
#define ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS(T, TArg1, ...) \
  template< typename TArg1 __VA_OPT__(, typename) __VA_ARGS__  > \
  class T; \
  template< typename TArg1 __VA_OPT__(, typename) __VA_ARGS__  > \
  using T##Ref = std::shared_ptr<T<TArg1 __VA_OPT__(,) __VA_ARGS__>>; \
  template< typename TArg1 __VA_OPT__(, typename) __VA_ARGS__  > \
  using T##WeakRef = std::weak_ptr<T<TArg1 __VA_OPT__(,) __VA_ARGS__ >>
#endif

/*!
 * Defines a dummy struct that can be used to make a public constructor unusable
 * outside the class. \sa ZYPP_ADD_CREATE_FUNC.
 */
#define ZYPP_ADD_PRIVATE_CONSTR_HELPER() \
  struct private_constr_t {  private_constr_t () noexcept = default; }

/*!
 * Use this to add the private constr argument to a constructor
 */
#define ZYPP_PRIVATE_CONSTR_ARG \
  private_constr_t

/*!
 * Use this to pass the private constr arg to a constructor
 */
#define ZYPP_PRIVATE_CONSTR_ARG_VAL \
  private_constr_t{}

/*!
 * Helper macro to add the default Class::create() static function
 * commonly used in libzypp.
 *
 * \code
 *
 * // always forward declare the Ref types so they can be used in the class
 * ZYPP_FWD_DECL_TYPE_WITH_REFS(MyClass)
 *
 * class MyClass
 * {
 *  // this adds the construct template function
 *  ZYPP_ADD_CREATE_FUNC(MyClass)
 *
 *  public:
 *    // adds a constructor that takes no args:
 *    // MyClass::create();
 *    ZYPP_DECL_PRIVATE_CONSTR(MyClass)
 *
 *    // adds a constructor with two arguments
 *    // MyClass::create( 10, "Hello");
 *    ZYPP_DECL_PRIVATE_CONSTR_ARGS(MyClass, int arg1, const std::string &label)
 * }
 *
 * ZYPP_IMPL_PRIVATE_CONSTR( MyClass ) {
 *  // do stuff
 * }
 *
 * ZYPP_IMPL_PRIVATE_CONSTR_ARGS( MyClass, int arg1, const std::string &label ) {
 *  // do stuff with args
 * }
 *
 * \endcode
 * \note requires the ref types for the class to be already defined
 * \TODO Add this to already existing objects
 */
#define ZYPP_ADD_CREATE_FUNC(Class) \
  private: \
    ZYPP_ADD_PRIVATE_CONSTR_HELPER(); \
  public: \
    template < typename ...Args > \
    inline static auto create ( Args &&... args ) { \
      return std::make_shared< Class >( private_constr_t{}, std::forward<Args>(args)... ); \
    } \
  private:

/*
 * Convenience macros to implement public but private constructors that can be called from Class::create() but
 * not by user code.
 *
 * \sa ZYPP_ADD_CONSTR_FUNC
 */
#define ZYPP_DECL_PRIVATE_CONSTR(Class) Class( private_constr_t )
#define ZYPP_IMPL_PRIVATE_CONSTR(Class) Class::Class( private_constr_t )
#define ZYPP_DECL_PRIVATE_CONSTR_ARGS(Class,...) Class( private_constr_t, __VA_ARGS__ )
#define ZYPP_IMPL_PRIVATE_CONSTR_ARGS(Class,...) Class::Class( private_constr_t, __VA_ARGS__ )

#define ZYPP_NODISCARD [[nodiscard]]

#endif
