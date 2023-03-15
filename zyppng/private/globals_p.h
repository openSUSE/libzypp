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

#include <zypp-core/zyppng/base/signals.h>
#include <cassert>
#include <glib.h>
#include <string>
#include <zyppng/utils/GObjectMemory>
#include <zypp-core/zyppng/base/zyppglobal.h>

#define ZYPP_WRAPPED_OBJ (zypp_wrapped_obj_quark ())
GQuark zypp_wrapped_obj_quark  (void);

namespace zyppng
{
  ZYPP_FWD_DECL_TYPE_WITH_REFS (Base);

  namespace internal {

    void registerWrapper ( const BaseRef &ref, GObject *wrapper );
    void unregisterWrapper ( const BaseRef &ref, GObject *wrapper );

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

  template <class PublicGObject, class Cpp, class CrtpBase>
  class WrapperPrivateBase
  {
  public:
    WrapperPrivateBase( PublicGObject *gObjPtr ) : _gObject(gObjPtr) { }

    void initialize () {
      if ( internal::WrapperRegistry<Cpp>::hasObj() )
        assignCppType( internal::WrapperRegistry<Cpp>::takeCppObj() );
      else {
        auto cppObj = static_cast<CrtpBase*>(this)->makeCppType();
        internal::registerWrapper ( cppObj, G_OBJECT(_gObject) );
        assignCppType ( std::move(cppObj) );
      }
    }

    virtual ~WrapperPrivateBase() {
      std::for_each ( _signalConns.begin (), _signalConns.end(), []( zyppng::connection &conn ) { conn.disconnect(); });
      if ( _wrappedObj )
        internal::unregisterWrapper ( _wrappedObj, G_OBJECT(_gObject) );
    }

  protected:
    virtual void assignCppType ( std::shared_ptr<Cpp> &&ptr ) {
      _wrappedObj = ptr;
    }

  public:
    PublicGObject *_gObject = nullptr;
    std::shared_ptr<Cpp> _wrappedObj;
    std::vector<zyppng::connection> _signalConns;
  };


  template <typename GObjType, typename Cpp >
  auto zypp_wrap_cpp( const std::shared_ptr<Cpp> &ref ) {
    void *ptr = ref->data( ZYPP_WRAPPED_OBJ );
    auto existingWrapper = zyppng::util::GLibTypeTraits<GObjType>::GObjectTrait::gobjectCast( G_OBJECT(ptr) );
    if ( existingWrapper ) {
      return zyppng::util::GObjectPtr<GObjType> ( existingWrapper, zyppng::retain_object );
    }

    g_assert( !ptr );

    internal::WrapperRegistry<Cpp>::setCppObj( ref );
    auto newObj = zyppng::util::g_object_create<GObjType>();

    // even if the C code did not take the ref make sure to clean it
    internal::WrapperRegistry<Cpp>::takeCppObj();

    return newObj;
  }

}

#endif
