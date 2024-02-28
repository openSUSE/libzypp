/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "userrequest.h"
#include <string>

namespace zyppng
{
  UserRequest::UserRequest( UserData userData )
    : _userData( std::move(userData) )
  { }

  UserRequest::~UserRequest()
  { }

  const UserData &UserRequest::userData() const
  {
    return _userData;
  }

  UserData &UserRequest::userData()
  {
    return _userData;
  }

  ZYPP_IMPL_PRIVATE_CONSTR_ARGS(ShowMessageRequest, const std::string &message, MType mType, const UserData &data )
    : UserRequest( data )
    , _type( mType )
    , _message( message )

  { }

  ZYPP_IMPL_PRIVATE_CONSTR_ARGS(ShowMessageRequest, std::string &&message, MType mType, UserData &&data )
    : UserRequest( std::move(data) )
    , _type( mType )
    , _message( std::move(message) )
  { }

  UserRequestType ShowMessageRequest::type() const
  {
    return UserRequestType::Message;
  }

  ShowMessageRequest::MType ShowMessageRequest::messageType()
  {
    return _type;
  }

  const std::string &ShowMessageRequest::message() const
  {
    return _message;
  }

  ZYPP_IMPL_PRIVATE_CONSTR_ARGS(ListChoiceRequest, const std::string &label, const std::vector<Choice> &answers, const index_type defaultAnswer, UserData userData )
    : UserRequest( std::move(userData) )
    , _label( label )
    , _answers( answers )
    , _answer( defaultAnswer )
    , _default( defaultAnswer )
  { }

  UserRequestType ListChoiceRequest::type() const
  {
    return UserRequestType::ListChoice;
  }

  const std::string &ListChoiceRequest::label() const
  {
    return _label;
  }

  void ListChoiceRequest::setChoice(const index_type sel )
  {
    if ( sel >= _answers.size() )
      ZYPP_THROW( std::logic_error("Selection index is out of range") );
    _answer = sel;
  }

  ListChoiceRequest::index_type ListChoiceRequest::choice() const
  {
    return _answer;
  }

  ListChoiceRequest::index_type ListChoiceRequest::defaultAnswer() const
  {
    return _default;
  }

  const std::vector<ListChoiceRequest::Choice> &ListChoiceRequest::answers() const
  {
    return _answers;
  }

  ZYPP_IMPL_PRIVATE_CONSTR_ARGS(BooleanChoiceRequest, const std::string &label, const bool defaultAnswer, UserData userData )
    : UserRequest( std::move(userData) )
    , _label( label ),
    _answer( defaultAnswer )
  { }

  UserRequestType BooleanChoiceRequest::type() const
  {
    return UserRequestType::BooleanChoice;
  }

  const std::string &BooleanChoiceRequest::label() const
  {
    return _label;
  }

  void BooleanChoiceRequest::setChoice(const bool sel)
  {
    _answer = sel;
  }

  bool BooleanChoiceRequest::choice() const
  {
    return _answer;
  }

}
