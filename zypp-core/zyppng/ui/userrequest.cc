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
#include <utility>

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

  ZYPP_IMPL_PRIVATE_CONSTR_ARGS(ShowMessageRequest, std::string message, MType mType, UserData data )
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

  ZYPP_IMPL_PRIVATE_CONSTR_ARGS(ListChoiceRequest, std::string label, std::vector<Choice> answers, index_type defaultAnswer, UserData userData )
    : UserRequest( std::move(userData) )
    , _label( std::move(label) )
    , _answers( std::move(answers) )
    , _answer( std::move(defaultAnswer) )
    , _default( std::move(defaultAnswer) )
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

    accept();
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

  ZYPP_IMPL_PRIVATE_CONSTR_ARGS(BooleanChoiceRequest, std::string label, const bool defaultAnswer, UserData userData )
    : UserRequest( std::move(userData) )
    , _label( std::move(label) )
    , _answer( defaultAnswer )
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
    accept();
    _answer = sel;
  }

  bool BooleanChoiceRequest::choice() const
  {
    return _answer;
  }

}
