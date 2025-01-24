/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_INPUT_REQUEST_P_H
#define ZYPP_GLIB_INPUT_REQUEST_P_H

#include <zypp-glib/ui/inputrequest.h>
#include <zypp-core/zyppng/ui/userrequest.h>
#include <zypp-glib/private/globals_p.h>

#include <optional>

struct _ZyppInputRequestField
{
  zyppng::InputRequest::Field _cppObj;
};

ZyppInputRequestField * zypp_wrap_cpp( zyppng::InputRequest::Field obj );


struct ZyppInputRequestPrivate
    : public zypp::glib::WrapperPrivateBase<ZyppInputRequest, zyppng::InputRequest, ZyppInputRequestPrivate>
{
  struct ConstructionProps {
    zyppng::InputRequestRef _cppObj;
  };

  std::optional<ConstructionProps> _constrProps = ConstructionProps();
  zyppng::InputRequestRef _cppObj;

  ZyppInputRequestPrivate( ZyppInputRequest *pub ) : WrapperPrivateBase(pub) {}

  void initializeCpp(){
    g_return_if_fail ( _constrProps.has_value() );

    if ( _constrProps->_cppObj ) {
      _cppObj = std::move( _constrProps->_cppObj );
    } else {
      g_error("It's not supported to create a ZyppInputRequest from C Code.");
    }
    _constrProps.reset();
  }
  zyppng::InputRequestRef &cppType()  {
    return _cppObj;
  }
};


struct _ZyppInputRequest
{
  GObjectClass parent_class;
};

/**
 * zypp_input_request_get_cpp: (skip)
 */
zyppng::InputRequestRef &zypp_input_request_get_cpp( ZyppInputRequest *self );



#endif // ZYPP_GLIB_INPUT_REQUEST_P_H
