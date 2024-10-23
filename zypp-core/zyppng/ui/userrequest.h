/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_CORE_ZYPPNG_USERREQUEST_H
#define ZYPP_CORE_ZYPPNG_USERREQUEST_H

#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/base/Signals>
#include <zypp-core/UserData.h>
#include <vector>
namespace zyppng {

  using UserData    = zypp::callback::UserData;
  using ContentType = zypp::ContentType;


  ZYPP_FWD_DECL_TYPE_WITH_REFS( UserRequest );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( ShowMessageRequest );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( ListChoiceRequest );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( BooleanChoiceRequest );

  /*
  constexpr std::string_view CTYPE_SHOW_MESSAGE_REQUEST ("userreq/show-message");
  constexpr std::string_view CTYPE_LIST_CHOICE_REQUEST  ("userreq/list-choice");
  constexpr std::string_view CTYPE_BOOLEAN_COICE_REQUEST("userreq/boolean-choice");
  */


  // keep in sync with glib wrapper code
  enum class UserRequestType : uint {
    Invalid,        // invalid message, used as a default return value in Glib Code
    Message,        // simple message to the user, no input or predefined like y/n
    ListChoice,     // request to select from a list of options
    BooleanChoice,  // request user to say yes or no
    Custom = 512
  };

  /*!
   * The UserRequest class provides a base object to send UI requests to the user,
   * every time libzypp needs input from the User, a UserRequest is generated and sent
   * via the \ref UserInterface to the user code, which can decide how to render the request,
   * a CLI app would render a prompt, while a UI application likely would open a popup window.
   */
  class UserRequest : public Base
  {
  public:
    UserRequest( UserData userData = {} );
    ~UserRequest() override;
    virtual UserRequestType type() const = 0;
    const UserData &userData() const;
    UserData &userData();

    /*!
     * Code accepted the event, it is now considered handled and will not be passed to the
     * parent evennt handler for processing.
     */
    void accept();

    /*!
     * Sets the internal accepted flag to false, event is now considered as "not yet handled"
     * and will bubble up to the parent event handler.
     */
    void ignore();

    /*!
     * Returns true if the event was handled, otherwise false is returned.
     */
    bool accepted() const;

  private:
    bool        _accepted = false;
    UserData    _userData;
  };



  /*!
   * The ShowMessageRequest class represents a simple informal message that the code needs to
   * print on the users screen
   *
   * \code
   * UserInterface::instance()->sendUserRequest( ShowMessageRequest::create("Repository downloaded") );
   * \endcode
   */
  class ShowMessageRequest : public UserRequest
  {
    ZYPP_ADD_CREATE_FUNC(ShowMessageRequest)
  public:

    // Keep in sync with the GLib wrapper enum!
    enum class MType {
      Debug, Info, Warning, Error, Important, Data
    };

    ZYPP_DECL_PRIVATE_CONSTR_ARGS(ShowMessageRequest, std::string message, MType mType = MType::Info, UserData data = {} );
    UserRequestType type() const override;

    MType messageType();
    const std::string &message() const;

  private:
    MType _type = MType::Info;
    std::string _message;
  };



  /*!
   * The ListChoice Request class represents a choice request to the user,
   * the possible answers are given via a vector of possible choices.
   *
   * \code
   *
   * auto request = ListChoiceRequest::create( "Please select the choice you want: ", { {"y", "Info about y"}, {"n", "Info about n"}, {"d", "Info about d"} }, 1 );
   * UserInterface::instance()->sendUserRequest( request );
   *
   * const auto choice = request->choice();
   * switch( choice ) {
   *  case 0:
   *   // handle 'y' choice
   *  break;
   *  case 1:
   *   // handle 'n' choice
   *  break;
   *  case 2:
   *    // handle 'd' choice
   *  break;
   *  default:
   *    // error, should not happen!
   *  break;
   * }
   *
   * \endcode
   */
  class ListChoiceRequest : public UserRequest
  {
    ZYPP_ADD_CREATE_FUNC(ListChoiceRequest)
  public:

    struct Choice {
      std::string opt;
      std::string detail;
    };

    using index_type = std::vector<Choice>::size_type;

    ZYPP_DECL_PRIVATE_CONSTR_ARGS(ListChoiceRequest, std::string label, std::vector<Choice> answers, index_type defaultAnswer, UserData userData = {} );
    UserRequestType type() const override;

    const std::string &label() const;

    void setChoice ( const index_type sel );
    index_type choice() const;

    index_type defaultAnswer() const;

    const std::vector<Choice> &answers() const;


  private:
    std::string _label;
    std::vector<Choice> _answers;
    index_type _answer = 0;
    index_type _default = 0;
  };


  class BooleanChoiceRequest : public UserRequest
  {
    ZYPP_ADD_CREATE_FUNC(BooleanChoiceRequest)

  public:
    ZYPP_DECL_PRIVATE_CONSTR_ARGS(BooleanChoiceRequest, std::string label, const bool defaultAnswer = false, UserData userData = {} );
    UserRequestType type() const override;

    const std::string &label() const;

    void setChoice ( const bool sel );
    bool choice() const;

  private:
    std::string _label;
    bool _answer = false;

  };

}


#endif
