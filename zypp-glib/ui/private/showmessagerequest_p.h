/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_SHOW_MESSAGE_REQUEST_P_H
#define ZYPP_GLIB_SHOW_MESSAGE_REQUEST_P_H

#include <zypp-glib/ui/showmessagerequest.h>
#include <optional>

#include <zypp-core/zyppng/ui/userrequest.h>
#include <zypp-glib/private/globals_p.h>

struct ZyppShowMsgRequestPrivate
    : public zypp::glib::WrapperPrivateBase<ZyppShowMsgRequest, zyppng::ShowMessageRequest, ZyppShowMsgRequestPrivate>
{
  struct ConstructionProps {
    zyppng::ShowMessageRequestRef _cppObj;
    std::optional<std::string> _label;
    std::optional<ZyppShowMsgRequestType> _type;
  };

  std::optional<ConstructionProps> _constrProps = ConstructionProps();
  zyppng::ShowMessageRequestRef _cppObj;

  ZyppShowMsgRequestPrivate( ZyppShowMsgRequest *pub ) : WrapperPrivateBase(pub) {}

  void initializeCpp(){
    g_return_if_fail ( _constrProps.has_value() );

    if ( _constrProps->_cppObj ) {
      _cppObj = std::move( _constrProps->_cppObj );
    } else {
      _cppObj = zyppng::ShowMessageRequest::create( _constrProps->_label.value_or(""), convertEnum( _constrProps->_type.value_or( ZYPP_SHOW_MSG_TYPE_INFO ) ));
    }
    _constrProps.reset();
  }
  zyppng::ShowMessageRequestRef &cppType()  {
    return _cppObj;
  }

  static zyppng::ShowMessageRequest::MType convertEnum( ZyppShowMsgRequestType t ) {
    switch(t){
      case ZYPP_SHOW_MSG_TYPE_DEBUG:
        return zyppng::ShowMessageRequest::MType::Debug;
      case ZYPP_SHOW_MSG_TYPE_INFO:
        return zyppng::ShowMessageRequest::MType::Info;
      case ZYPP_SHOW_MSG_TYPE_WARNING:
        return zyppng::ShowMessageRequest::MType::Warning;
      case ZYPP_SHOW_MSG_TYPE_ERROR:
        return zyppng::ShowMessageRequest::MType::Error;
      case ZYPP_SHOW_MSG_TYPE_IMPORTANT:
        return zyppng::ShowMessageRequest::MType::Important;
      case ZYPP_SHOW_MSG_TYPE_DATA:
        return zyppng::ShowMessageRequest::MType::Data;
        break;
    }

    ERR << "Unknown message type " << (int)t << std::endl;
    return zyppng::ShowMessageRequest::MType::Info;
  }

  static ZyppShowMsgRequestType convertEnum( zyppng::ShowMessageRequest::MType t ) {
    switch(t){
      case zyppng::ShowMessageRequest::MType::Debug:
        return ZYPP_SHOW_MSG_TYPE_DEBUG;
      case zyppng::ShowMessageRequest::MType::Info:
        return ZYPP_SHOW_MSG_TYPE_INFO;
      case zyppng::ShowMessageRequest::MType::Warning:
        return ZYPP_SHOW_MSG_TYPE_WARNING;
      case zyppng::ShowMessageRequest::MType::Error:
        return ZYPP_SHOW_MSG_TYPE_ERROR;
      case zyppng::ShowMessageRequest::MType::Important:
        return ZYPP_SHOW_MSG_TYPE_IMPORTANT;
      case zyppng::ShowMessageRequest::MType::Data:
        return ZYPP_SHOW_MSG_TYPE_DATA;
        break;
    }

    ERR << "Unknown message type " << (int)t << std::endl;
    return ZYPP_SHOW_MSG_TYPE_INFO;
  }

};


struct _ZyppShowMsgRequest
{
  GObjectClass parent_class;
};

/**
 * zypp_show_message_request_get_cpp: (skip)
 */
zyppng::ShowMessageRequestRef &zypp_show_message_request_get_cpp( ZyppShowMsgRequest *self );


#endif // ZYPP_GLIB_SHOW_MESSAGE_REQUEST_P_H
