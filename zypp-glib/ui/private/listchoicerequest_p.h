/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_LIST_CHOICE_REQUEST_P_H
#define ZYPP_GLIB_LIST_CHOICE_REQUEST_P_H

#include <zypp-glib/ui/listchoicerequest.h>
#include <zypp-core/zyppng/ui/userrequest.h>
#include <zypp-glib/private/globals_p.h>

#include <optional>

struct _ZyppListChoiceOption
{
  zyppng::ListChoiceRequest::Choice _cppObj;
};

ZyppListChoiceOption * zypp_wrap_cpp( zyppng::ListChoiceRequest::Choice obj );

struct ZyppListChoiceRequestPrivate
    : public zypp::glib::WrapperPrivateBase<ZyppListChoiceRequest, zyppng::ListChoiceRequest, ZyppListChoiceRequestPrivate>
{
  struct ConstructionProps {
    zyppng::ListChoiceRequestRef _cppObj;
  };

  std::optional<ConstructionProps> _constrProps = ConstructionProps();
  zyppng::ListChoiceRequestRef _cppObj;

  ZyppListChoiceRequestPrivate( ZyppListChoiceRequest *pub ) : WrapperPrivateBase(pub) {}

  void initializeCpp(){
    g_return_if_fail ( _constrProps.has_value() );

    if ( _constrProps->_cppObj ) {
      _cppObj = std::move( _constrProps->_cppObj );
    } else {
      g_error("It's not supported to create a ZyppListChoiceRequest from C Code.");
    }
    _constrProps.reset();
  }
  zyppng::ListChoiceRequestRef &cppType()  {
    return _cppObj;
  }
};


struct _ZyppListChoiceRequest
{
  GObjectClass parent_class;
};

/**
 * zypp_list_choice_request_get_cpp: (skip)
 */
zyppng::ListChoiceRequestRef &zypp_list_choice_request_get_cpp( ZyppListChoiceRequest *self );



#endif
