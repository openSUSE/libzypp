/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef GLOBALS_P_H
#define GLOBALS_P_H

#include "zypp-core/AutoDispose.h"
#include <zypp-core/zyppng/base/signals.h>
#include <glib.h>
#include <zypp-glib/utils/GObjectMemory>
#include <zypp-core/zyppng/base/zyppglobal.h>

#define ZYPP_WRAPPED_OBJ (zypp_wrapped_obj_quark ())
GQuark zypp_wrapped_obj_quark  (void);

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS (Base);
}

namespace zypp::glib
{

  namespace internal {

    constexpr std::string_view ZYPP_CPP_OBJECT_PROPERTY_NAME("zypp_cppObj");

    void registerWrapper ( const zyppng::BaseRef &ref, GObject *wrapper );
    void unregisterWrapper ( const zyppng::BaseRef &ref, GObject *wrapper );

    template <typename T>
    class WrapperRegistry
    {
      static std::shared_ptr<T> &storage ()
      {
        static thread_local std::shared_ptr<T> globObj;
        return globObj;
      }

    public:
      static bool hasObj ()
      {
        return storage() ? true : false;
      }

      static void setCppObj ( const std::shared_ptr<T> &obj )
      {
        auto &ref = storage();
        g_assert(!ref);
        ref = obj;
      }

      static std::shared_ptr<T> takeCppObj ()
      {
        auto objCopy = storage();
        storage().reset();
        return objCopy;
      }
    };
  }

  /*!
   * Helper class for a private GObject implementation for a Cpp wrapper type,
   * the subclass needs to have the methods:
   *
   * - void initializeCpp( )
   *   If no construct cppObj property is set creates a new instance of the cpp type and
   *   connects signals etc, this is the case whenever the GObject type is created and there is no
   *   Cpp instance that should be wrapped. If a onstruct cppObj property is set a already existing and intialized
   *   cpp Object is to be used.
   *
   * - std::shared_ptr<Cpp> &cppType()
   *   Return the current cpp instance
   *
   * The GObject type needs to have a private that is derived from WrapperPrivateBase e.g.:
   * \code
   * struct MyClassPrivate
   *   : public zypp::glib::WrapperPrivateBase<MyClass, MyClassCpp, MyClassPrivate>
   * {
   *  MyClassPrivate( MyClass *pub ) : WrapperPrivateBase(pub) { };
   *
   *  // automatically called by the Wrapper superclass
   *  void initializeCpp() {
   *    if ( _constructProps && _constructProps._cppObj ) {
   *      // we got a ready cpp Object, ignore all other construct properties
   *      _cppRef = std::move( _constructProps._cppObj );
   *    } else {
   *      // custom code to initialize
   *      _cppRef = MyClassCpp::create();
   *    }
   *    _contructProps.reset(); // don't need those anymore
   *
   *    // here connect signals etc
   *    ...
   *  }
   *
   *  MyClassCppRef &cppType() {
   *    return _cppRef;
   *  }
   *
   *  // a place to store construct only properties
   *  struct ConstructionProps {
   *    MyClassCppRef _cppObj;
   *  };
   *
   *  // those properties are set at construction right away, and will be unset
   *  // as soon as they have been consumed
   *  std::optional<ConstructionProps> _constructProps = ConstructionProps();
   *  MyClassCppRef _cppRef;
   * };
   * \endcode
   *
   * Have a construction property that can hold the Cpp reference, sadly GObject construction
   * is somewhat different to C++ so that contructor arguments/properties are set one by one
   * with the set_property() functions.. however C++ requires its constructor properties right away.
   * So we have to store the construct only properties somewhere and then query them later
   *
   * \code
   *
   * typedef enum
   * {
   *   PROP_CPPOBJ  = 1,
   *   N_PROPERTIES
   * } MyClassProperty;
   *
   * void my_class_class_init( ZyppContextClass *klass )
   * {
   *   GObjectClass *object_class = G_OBJECT_CLASS(klass);
   *   object_class->constructed  = my_class_constructed;
   *   object_class->finalize     = my_class_finalize;
   *
   *   object_class->set_property = my_class_set_property;
   *   object_class->get_property = my_class_get_property;
   *   obj_properties[PROP_CPPOBJ]  = ZYPP_GLIB_ADD_CPPOBJ_PROP();
   *
   *   // Add other properties ......
   *
   *   g_object_class_install_properties (object_class,
   *                                      N_PROPERTIES,
   *                                      obj_properties);
   * }
   *
   * static void
   * my_class_class_set_property (GObject      *object,
   *                            guint         property_id,
   *                            const GValue *value,
   *                            GParamSpec   *pspec)
   * {
   *   MyClass *self = MY_CLASS (object);
   *   ZYPP_GLIB_WRAPPER_D( MyClass, my_class );
   *
   *   switch ((MyClassProperty)property_id )
   *     {
   *     case PROP_CPPOBJ: ZYPP_GLIB_SET_CPPOBJ_PROP( MyClassCpp, value, d->_constructProps->_cppObj)
   *     default:
   *       // We don't have any other property...
   *       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
   *       break;
   *     }
   * }
   *
   *
   * \endcode
   *
   * Finally initialize the private in its own type_name_init() and type_name_constructed() function:
   * \code
   * static void my_class_init( MyClass *self )
   * {
   *   // call C++ constructors for the Private
   *   ZYPP_GLIB_CONSTRUCT_WRAPPER_PRIVATE(MyClass, my_class);
   * }
   *
   * static void my_class_constructed ( MyClass *self )
   * {
   *   // call initialize() to signal that we have everything to create
   *   // or pick up our cpp object
   *   ZYPP_GLIB_INIT_WRAPPER_PRIVATE(MyClass, my_class);
   * }
   *
   * static void my_class_finalize (GObject *gobject)
   * {
   *   auto *self = MY_CLASS (gobject);
   *
   *   ZYPP_GLIB_FINALIZE_WRAPPER_PRIVATE( MyClass, my_class )
   *
   *   // always call parent class finalizer
   *   G_OBJECT_CLASS (my_class_parent_class)->finalize (gobject);
   * }
   * \endcode
   *
   */
  template <class PublicGObject, class Cpp, class CrtpBase>
  class WrapperPrivateBase
  {

  public:
    WrapperPrivateBase(PublicGObject *gObjPtr) : _gObject(gObjPtr) {}
    WrapperPrivateBase(const WrapperPrivateBase &) = delete;
    WrapperPrivateBase(WrapperPrivateBase &&) = delete;
    WrapperPrivateBase &operator=(const WrapperPrivateBase &) = delete;
    WrapperPrivateBase &operator=(WrapperPrivateBase &&) = delete;
    virtual ~WrapperPrivateBase() = default;

    void initialize () {
      static_cast<CrtpBase*>(this)->initializeCpp();
      const auto &ptr = static_cast<CrtpBase*>(this)->cppType();
      if ( !ptr )
        g_error("cppType() must return a instance at this point. This is a bug!");

      internal::registerWrapper ( static_cast<CrtpBase*>(this)->cppType(), G_OBJECT(_gObject) );
    }

    void finalize() {
      std::for_each ( _signalConns.begin (), _signalConns.end(), []( zyppng::connection &conn ) { conn.disconnect(); });
      internal::unregisterWrapper ( static_cast<CrtpBase*>(this)->cppType(), G_OBJECT(_gObject) );
    }

  public:
    PublicGObject *_gObject = nullptr;
    std::vector<zyppng::connection> _signalConns;
  };


  template <typename GObjType, typename Cpp >
  auto zypp_wrap_cpp( const std::shared_ptr<Cpp> &ref ) {
    void *ptr = ref->data( ZYPP_WRAPPED_OBJ );
    auto existingWrapper = zypp::glib::GLibTypeTraits<GObjType>::GObjectTrait::gobjectCast( G_OBJECT(ptr) );
    if ( existingWrapper ) {
      return zypp::glib::GObjectPtr<GObjType> ( existingWrapper, retain_object );
    }

    g_assert( !ptr );
    return zypp::glib::g_object_create<GObjType>( internal::ZYPP_CPP_OBJECT_PROPERTY_NAME.data(), (void *)&ref );
  }

}

/**
 * Equivalent of Z_D() in the glib world, creates a pointer to the private data called \a d
 */
#define ZYPP_GLIB_WRAPPER_D( TypeName, type_name ) \
  auto d = static_cast<TypeName##Private *>(type_name##_get_instance_private( self ))

#define ZYPP_GLIB_CONSTRUCT_WRAPPER_PRIVATE( TypeName, type_name ){ \
    ZYPP_GLIB_WRAPPER_D( TypeName, type_name ); \
    new( d ) TypeName##Private( self ); /*call all constructors*/ \
  }

#define ZYPP_GLIB_INIT_WRAPPER_PRIVATE( TypeName, type_name ){ \
    ZYPP_GLIB_WRAPPER_D( TypeName, type_name ); \
    d->initialize(); \
  }

#define ZYPP_GLIB_FINALIZE_WRAPPER_PRIVATE( TypeName, type_name ){ \
    ZYPP_GLIB_WRAPPER_D( TypeName, type_name ); \
    d->finalize(); \
    d->~TypeName##Private(); \
  }

#define ZYPP_GLIB_ADD_CPPOBJ_PROP() \
  g_param_spec_pointer ( ::zypp::glib::internal::ZYPP_CPP_OBJECT_PROPERTY_NAME.data(), \
                         nullptr, \
                         nullptr, \
                         GParamFlags( G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE) )

#define ZYPP_GLIB_SET_CPPOBJ_PROP(  CppType, value, target ) { \
  gpointer obj = g_value_get_pointer( value ); \
  if ( obj ) \
    target = *( reinterpret_cast<CppType*>(obj) ); \
  break; \
  }


/**
 * Helper macros to reduce the boilerplate code we'd need to repeat over and
 * over again. This is to be used with the \ref zypp::glib:WrapperPrivateBase and
 * a GObject with a instance private. \ref G_DEFINE_TYPE_WITH_PRIVATE.
 * See \ref ZyppContext as a example.
 */
#define ZYPP_DECLARE_GOBJECT_BOILERPLATE( TypeName, type_name ) \
  static void type_name##_init( TypeName *self ); \
  static void type_name##_constructed ( GObject *gobject ); \
  static void type_name##_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec ); \
  static void type_name##_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec ); \
  static void type_name##_finalize (GObject *gobject); \
  static TypeName##Private * type_name##_get_private( TypeName *self );

#define ZYPP_DEFINE_GOBJECT_BOILERPLATE( ModuleObjName, module_obj_name, MODULE, OBJ_NAME ) \
  static void module_obj_name##_init( ModuleObjName *self )\
  {\
    ZYPP_GLIB_CONSTRUCT_WRAPPER_PRIVATE(ModuleObjName, module_obj_name);\
  }\
  \
  static void module_obj_name##_constructed ( GObject *gobject ) { \
    g_return_if_fail( (gobject != nullptr) && MODULE##_IS_##OBJ_NAME(gobject) ); \
  \
    /* Call the parent class's constructed method */ \
    G_OBJECT_CLASS(module_obj_name##_parent_class)->constructed(gobject); \
  \
    ModuleObjName *self = MODULE##_##OBJ_NAME(gobject); \
    ZYPP_GLIB_INIT_WRAPPER_PRIVATE(ModuleObjName, module_obj_name); \
  }\
  \
  static void module_obj_name##_finalize (GObject *gobject) \
  { \
    ModuleObjName *self = MODULE##_##OBJ_NAME(gobject); \
  \
    ZYPP_GLIB_FINALIZE_WRAPPER_PRIVATE( ModuleObjName, module_obj_name ) \
  \
    /* always call parent class finalizer*/ \
    G_OBJECT_CLASS (module_obj_name##_parent_class)->finalize (gobject); \
  } \
  \
  static ModuleObjName##Private * module_obj_name##_get_private( ModuleObjName *self ) { \
    return static_cast<ModuleObjName##Private *>( module_obj_name##_get_instance_private( self ) ) ; \
  }

#define ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS( module_obj_name, object_class ) \
  object_class->constructed  = module_obj_name##_constructed; \
  object_class->finalize     = module_obj_name##_finalize; \
  object_class->set_property = module_obj_name##_set_property; \
  object_class->get_property = module_obj_name##_get_property

#endif
