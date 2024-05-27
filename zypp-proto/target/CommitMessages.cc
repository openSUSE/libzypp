/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "CommitMessages.h"

#include <zypp-core/TriBool.h>
#include <zypp-core/zyppng/core/String>
#include <string>
#include <string_view>

namespace zypp::proto::target
{
  namespace {

    // Reads data from the stomp message and converts it to the target type
    // used to read header values and values serialized into a terminated data field
    template <typename T>
    void parseDataIntoField( const std::string &headerVal, T &target  )
    {
      if constexpr ( std::is_same_v<bool, T> ) {

        const auto &triBool = str::strToTriBool ( headerVal );
        if ( indeterminate(triBool) ) {
          ZYPP_THROW ( zypp::PluginFrameException( "Invalid value for boolean field" ) );
        }
        target = bool(triBool);
      } else if constexpr ( std::is_same_v<std::string, T> ) {
        target = headerVal;
      } else {
        // numbers
        auto val = zyppng::str::safe_strtonum<T> ( headerVal );
        if ( !val )
          ZYPP_THROW ( zypp::PluginFrameException( "Invalid value for numerical field" ) );
        target = *val;
      }
    }

    template <typename T>
    void parseHeaderIntoField( const zypp::PluginFrame &msg, const std::string &name, T &target  )
    {
      return parseDataIntoField ( msg.getHeader(name), target );
    }

    // Serializes a field of type T into the ByteArray buffer, appending a \0 to terminate the field
    template <typename T>
    void serializeStepField ( ByteArray &data, const T&field ) {
      if constexpr ( std::is_same_v<bool, T> ) {
        if ( field ) {
          data.push_back ('1');
          data.push_back ('\0');
        } else {
          data.push_back ('0');
          data.push_back ('\0');
        }
      } else if constexpr ( std::is_same_v<std::string, T> ) {
        data.insert(data.end(), field.begin(), field.end() );
        data.push_back ('\0');
      } else {
        // numbers, convert to string and recursively call again
        serializeStepField (data, str::numstring (field) );
      }
    }

    // fetches a terminated string from the iterator, requires the terminator to come up before hitting the \a end
    std::string fetchTerminatedField ( ByteArray::const_iterator &i, ByteArray::const_iterator end, const char term = '\0' ){
      std::string fData;
      fData.reserve(512);
      while ( true ) {
        if ( i == end )
          ZYPP_THROW( zypp::PluginFrameException("Unexpected end of field in transaction step") );
        if ( *i == term ) {
          i++; // consume the terminator
          break;
        }
        fData.push_back( *i );
        i++;
      }

      fData.shrink_to_fit();
      return fData;
    };

    template <typename T>
    using has_stepid = decltype(std::declval<T>().stepId);

    template <typename T>
    zyppng::expected<PluginFrame> makeTrivialMessage( const std::string &command, const T& msg ) {

      PluginFrame f(command);
      if constexpr ( std::is_detected_v<has_stepid, T> )
          f.addHeader ("stepId", asString (msg.stepId) );
      return zyppng::expected<PluginFrame>::success ( std::move(f) );
    }

    template <typename T>
    zyppng::expected<T> parseTrivialMessage( const PluginFrame &msg ) {
      try {
        T c;
        if ( msg.command() != T::typeName )
          return zyppng::expected<T>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException( str::Str()<<"Message is not a " << T::typeName ) ) );

        if constexpr ( std::is_detected_v<has_stepid, T> ) {
          parseHeaderIntoField ( msg, "stepId", c.stepId );
        }

        return zyppng::expected<T>::success(c);

      } catch( const zypp::Exception &e ) {
        ZYPP_CAUGHT (e);
        return zyppng::expected<T>::error( ZYPP_EXCPT_PTR(e) );
      }
    }

    template <typename T>
    inline PluginFrame prepareFrame() {
      return PluginFrame ( std::string( T::typeName.data(), T::typeName.length() ) );
    }
  }

#define IMPL_TRIVIAL_MESSAGE( Type ) \
  zyppng::expected<PluginFrame> Type::toStompMessage() const \
  { \
    return makeTrivialMessage( #Type, *this ); \
  } \
  \
  zyppng::expected<Type> Type::fromStompMessage(const PluginFrame &msg) \
  { \
    return parseTrivialMessage<Type>( msg ); \
  }

  zyppng::expected<PluginFrame> Commit::toStompMessage() const
  {
    PluginFrame f = prepareFrame<Commit>();
    f.addHeader ("flags", asString (flags) );
    f.addHeader ("arch", arch );
    f.addHeader ("root", root );
    f.addHeader ("dbPath", dbPath );
    f.addHeader ("lockFilePath", lockFilePath );
    f.addHeader ("ignoreArch", ignoreArch ? "1" : "0" );

    ByteArray &body = f.bodyRef();
    for ( const auto &step : transactionSteps ) {
      if ( std::holds_alternative<InstallStep>(step) ) {
        const InstallStep &value = std::get<InstallStep>(step);
        body.push_back( InstallStepType );
        serializeStepField( body, value.stepId );
        serializeStepField( body, value.pathname );
        serializeStepField( body, value.multiversion );

      } else if ( std::holds_alternative<RemoveStep>(step) ) {
        const RemoveStep &value = std::get<RemoveStep>(step);
        body.push_back( RemoveStepType );
        serializeStepField( body, value.stepId  );
        serializeStepField( body, value.name    );
        serializeStepField( body, value.version );
        serializeStepField( body, value.release );
        serializeStepField( body, value.arch    );
      } else {
        return zyppng::expected<PluginFrame>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Unknown Step type in message") ) );
      }
    }
    body.shrink_to_fit();
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<Commit> Commit::fromStompMessage( const zypp::PluginFrame &msg )
  {
    try {
      Commit c;

      if ( msg.command() != Commit::typeName )
        return zyppng::expected<Commit>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a Commit") ) );

      parseHeaderIntoField ( msg, "flags", c.flags );
      parseHeaderIntoField ( msg, "arch", c.arch );
      parseHeaderIntoField ( msg, "root", c.root );
      parseHeaderIntoField ( msg, "dbPath", c.dbPath );
      parseHeaderIntoField ( msg, "lockFilePath", c.lockFilePath );
      parseHeaderIntoField ( msg, "ignoreArch", c.ignoreArch );

      // we got the fields, lets parse the steps
      // steps are serialized into a very simple form, starting with one byte that tells us what step type we look at
      // <stepType><step fields seperated by \0>
      enum ParseState {
        ReadingStepType,
        Reading
      };

      const auto &data = msg.body ();
      for ( auto i = data.begin(); i != data.end(); ) {
        uint8_t stepType = *i;
        i++;

        if ( i == data.end() )
          return zyppng::expected<Commit>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Invalid data in commit message") ) );

        switch ( stepType ) {
          case InstallStepType: {
            InstallStep s;
            parseDataIntoField( fetchTerminatedField(i, data.end() ), s.stepId       );
            parseDataIntoField( fetchTerminatedField(i, data.end() ), s.pathname     );
            parseDataIntoField( fetchTerminatedField(i, data.end() ), s.multiversion );
            c.transactionSteps.push_back(s);
            break;
          }
          case RemoveStepType: {
            RemoveStep s;
            parseDataIntoField( fetchTerminatedField(i, data.end() ), s.stepId  );
            parseDataIntoField( fetchTerminatedField(i, data.end() ), s.name    );
            parseDataIntoField( fetchTerminatedField(i, data.end() ), s.version );
            parseDataIntoField( fetchTerminatedField(i, data.end() ), s.release );
            parseDataIntoField( fetchTerminatedField(i, data.end() ), s.arch    );
            c.transactionSteps.push_back(s);
            break;
          }
          default:
            return zyppng::expected<Commit>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Invalid step type in commit message") ) );
        }
      }

      return zyppng::expected<Commit>::success ( std::move(c) );

    } catch( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<Commit>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  zyppng::expected<PluginFrame> TransactionError::toStompMessage() const
  {
    PluginFrame f = prepareFrame<TransactionError>();
    ByteArray &body = f.bodyRef();
    for ( const auto &err : problems ) {
      serializeStepField ( body, err );
    }
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<TransactionError> TransactionError::fromStompMessage(const PluginFrame &msg)
  {
    try {
      TransactionError c;
      if ( msg.command() != TransactionError::typeName )
        return zyppng::expected<TransactionError>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a TransactionError") ) );

      const auto &data = msg.body ();
      for ( auto i = data.begin(); i != data.end(); ) {
        c.problems.push_back( fetchTerminatedField( i, data.end() ) );
      }

      return zyppng::expected<TransactionError>::success( std::move(c) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<TransactionError>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  zyppng::expected<PluginFrame> RpmLog::toStompMessage() const
  {
    PluginFrame f = prepareFrame<RpmLog>();
    f.addHeader ("level", asString (level) );
    f.setBody( line );
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<RpmLog> RpmLog::fromStompMessage(const PluginFrame &msg)
  {
    try {
      RpmLog c;
      if ( msg.command() != RpmLog::typeName )
        return zyppng::expected<RpmLog>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a RpmLog") ) );

      parseHeaderIntoField ( msg, "level", c.level );
      c.line = msg.body().asString();

      return zyppng::expected<RpmLog>::success( std::move(c) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<RpmLog>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  IMPL_TRIVIAL_MESSAGE(PackageBegin)
  IMPL_TRIVIAL_MESSAGE(PackageFinished)
  IMPL_TRIVIAL_MESSAGE(PackageError)

  zyppng::expected<PluginFrame> PackageProgress::toStompMessage() const
  {
    PluginFrame f = prepareFrame<PackageProgress>();
    f.addHeader ("stepId", asString (stepId) );
    f.addHeader ("amount", asString (amount) );
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<PackageProgress> PackageProgress::fromStompMessage(const PluginFrame &msg)
  {
    try {
      PackageProgress c;
      if ( msg.command() != PackageProgress::typeName )
        return zyppng::expected<PackageProgress>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a PackageProgress") ) );

      parseHeaderIntoField ( msg, "stepId", c.stepId );
      parseHeaderIntoField ( msg, "amount", c.amount );

      return zyppng::expected<PackageProgress>::success( std::move(c) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<PackageProgress>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  zyppng::expected<PluginFrame> CleanupBegin::toStompMessage() const
  {
    PluginFrame f = prepareFrame<CleanupBegin>();
    f.addHeader ("nvra", asString (nvra) );
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<CleanupBegin> CleanupBegin::fromStompMessage(const PluginFrame &msg)
  {
    try {
      CleanupBegin c;
      if ( msg.command() != CleanupBegin::typeName )
        return zyppng::expected<CleanupBegin>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a CleanupBegin") ) );

      parseHeaderIntoField ( msg, "nvra", c.nvra );
      return zyppng::expected<CleanupBegin>::success( std::move(c) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<CleanupBegin>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  zyppng::expected<PluginFrame> CleanupFinished::toStompMessage() const
  {
    PluginFrame f = prepareFrame<CleanupFinished>();
    f.addHeader ("nvra", asString (nvra) );
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<CleanupFinished> CleanupFinished::fromStompMessage(const PluginFrame &msg)
  {
    try {
      CleanupFinished c;
      if ( msg.command() != CleanupFinished::typeName )
        return zyppng::expected<CleanupFinished>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a CleanupFinished") ) );

      parseHeaderIntoField ( msg, "nvra", c.nvra );
      return zyppng::expected<CleanupFinished>::success( std::move(c) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<CleanupFinished>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  zyppng::expected<PluginFrame> CleanupProgress::toStompMessage() const
  {
    PluginFrame f = prepareFrame<CleanupProgress>();
    f.addHeader ("nvra", asString (nvra) );
    f.addHeader ("amount", asString (amount) );
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<CleanupProgress> CleanupProgress::fromStompMessage(const PluginFrame &msg)
  {
    try {
      CleanupProgress c;
      if ( msg.command() != CleanupProgress::typeName )
        return zyppng::expected<CleanupProgress>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a CleanupProgress") ) );

      parseHeaderIntoField ( msg, "nvra", c.nvra );
      parseHeaderIntoField ( msg, "amount", c.amount );
      return zyppng::expected<CleanupProgress>::success( std::move(c) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<CleanupProgress>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  zyppng::expected<PluginFrame> ScriptBegin::toStompMessage() const
  {
    PluginFrame f = prepareFrame<ScriptBegin>();
    f.addHeader ("stepId"       , asString (stepId) );
    f.addHeader ("scriptType"   , asString (scriptType) );
    f.addHeader ("scriptPackage", asString (scriptPackage) );
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<ScriptBegin> ScriptBegin::fromStompMessage(const PluginFrame &msg)
  {
    try {
      ScriptBegin c;
      if ( msg.command() != ScriptBegin::typeName )
        return zyppng::expected<ScriptBegin>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a ScriptBegin") ) );

      parseHeaderIntoField ( msg, "stepId"        , c.stepId );
      parseHeaderIntoField ( msg, "scriptType"    , c.scriptType );
      parseHeaderIntoField ( msg, "scriptPackage" , c.scriptPackage );

      return zyppng::expected<ScriptBegin>::success( std::move(c) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<ScriptBegin>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  IMPL_TRIVIAL_MESSAGE(ScriptFinished)

  zyppng::expected<PluginFrame> ScriptError::toStompMessage() const
  {
    PluginFrame f = prepareFrame<ScriptError>();
    f.addHeader ("stepId" , asString (stepId) );
    f.addHeader ("fatal"  , asString (fatal) );
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<ScriptError> ScriptError::fromStompMessage(const PluginFrame &msg)
  {
    try {
      ScriptError c;
      if ( msg.command() != ScriptError::typeName )
        return zyppng::expected<ScriptError>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a ScriptError") ) );

      parseHeaderIntoField ( msg, "stepId"  , c.stepId );
      parseHeaderIntoField ( msg, "fatal"   , c.fatal );

      return zyppng::expected<ScriptError>::success( std::move(c) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<ScriptError>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  zyppng::expected<PluginFrame> TransBegin::toStompMessage() const
  {
    PluginFrame f = prepareFrame<TransBegin>();
    f.addHeader ("name" , asString (name) );
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<TransBegin> TransBegin::fromStompMessage(const PluginFrame &msg)
  {
    try {
      TransBegin c;
      if ( msg.command() != TransBegin::typeName )
        return zyppng::expected<TransBegin>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a TransBegin") ) );

      parseHeaderIntoField ( msg, "name"  , c.name );

      return zyppng::expected<TransBegin>::success( std::move(c) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<TransBegin>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  IMPL_TRIVIAL_MESSAGE( TransFinished );

  zyppng::expected<PluginFrame> TransProgress::toStompMessage() const
  {
    PluginFrame f = prepareFrame<TransProgress>();
    f.addHeader ("amount", asString (amount) );
    return zyppng::expected<PluginFrame>::success ( std::move(f) );
  }

  zyppng::expected<TransProgress> TransProgress::fromStompMessage(const PluginFrame &msg)
  {
    try {
      TransProgress c;
      if ( msg.command() != TransProgress::typeName )
        return zyppng::expected<TransProgress>::error( ZYPP_EXCPT_PTR( zypp::PluginFrameException("Message is not a TransProgress") ) );

      parseHeaderIntoField ( msg, "amount", c.amount );

      return zyppng::expected<TransProgress>::success( std::move(c) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<TransProgress>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

}
