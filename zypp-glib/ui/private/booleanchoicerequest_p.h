/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_BOOLEAN_CHOICE_REQUEST_P_H
#define ZYPP_GLIB_BOOLEAN_CHOICE_REQUEST_P_H

#include <zypp-glib/ui/booleanchoicerequest.h>
#include <zypp-core/zyppng/ui/userrequest.h>
#include <zypp-glib/private/globals_p.h>

#include <optional>


struct ZyppBooleanChoiceRequestPrivate
    : public zypp::glib::WrapperPrivateBase<ZyppBooleanChoiceRequest, zyppng::BooleanChoiceRequest, ZyppBooleanChoiceRequestPrivate>
{
  struct ConstructionProps {
    zyppng::BooleanChoiceRequestRef _cppObj;
  };

  std::optional<ConstructionProps> _constrProps = ConstructionProps();
  zyppng::BooleanChoiceRequestRef _cppObj;

  ZyppBooleanChoiceRequestPrivate( ZyppBooleanChoiceRequest *pub ) : WrapperPrivateBase(pub) {}

  void initializeCpp(){
    g_return_if_fail ( _constrProps.has_value() );

    if ( _constrProps->_cppObj ) {
      _cppObj = std::move( _constrProps->_cppObj );
    } else {
      g_error("It's not supported to create a ZyppBooleanChoiceRequest from C Code.");
    }
    _constrProps.reset();
  }
  zyppng::BooleanChoiceRequestRef &cppType()  {
    return _cppObj;
  }
};


struct _ZyppBooleanChoiceRequest
{
  GObjectClass parent_class;
};

/**
 * zypp_boolean_choice_request_get_cpp: (skip)
 */
zyppng::BooleanChoiceRequestRef &zypp_boolean_choice_request_get_cpp( ZyppBooleanChoiceRequest *self );



#endif
