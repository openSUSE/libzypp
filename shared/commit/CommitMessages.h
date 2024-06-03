/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_SHARED_COMMIT_COMMITMESSAGE_H_INCLUDED
#define ZYPP_SHARED_COMMIT_COMMITMESSAGE_H_INCLUDED

#include <zypp-core/rpc/PluginFrame.h>
#include <zypp-core/zyppng/pipelines/expected.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

/*!
 * This file specifies all messages that are sent between TargetImpl and the zypp-rpm
 * backend binary. The communication protocol uses our implementation of the
 * STOMP protocol mesage format. \sa zypp::PluginFrame
 */
namespace zypp::proto::target
{

  // transaction steps are sent as part of the Commit Message
  // serialized to 0x0\stepId\0pathName\0multiversion\0
  struct InstallStep {
    uint32_t stepId;
    std::string pathname;
    bool   multiversion;
  };

  // serialized to 0x1stepId\0name\0version\0release\0arch\0
  struct RemoveStep {
    uint32_t stepId;
    std::string name;
    std::string version;
    std::string release;
    std::string arch   ;
  };

  using TransactionStep = std::variant<InstallStep,RemoveStep>;

  // first message that is sent to zypp-rpm
  // to setup the commit basics.
  struct Commit
  {
    Commit() = default;
    Commit(const Commit &) = default;
    Commit(Commit &&) = default;
    Commit &operator=(const Commit &) = default;
    Commit &operator=(Commit &&) = default;

    static constexpr std::string_view typeName = "Commit";
    enum StepTypes : uint8_t {
      InstallStepType,
      RemoveStepType
    };

    uint32_t flags;
    std::string arch;
    std::string root;
    std::string dbPath;
    std::string lockFilePath;
    bool   ignoreArch;
    std::vector<TransactionStep> transactionSteps;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<Commit> fromStompMessage( const zypp::PluginFrame &msg );
  };


  // message written to zypper when the transaction has failed
  struct TransactionError {
    static constexpr std::string_view typeName = "TransactionError";
    std::vector<std::string> problems;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<TransactionError> fromStompMessage( const zypp::PluginFrame &msg );
  };

  // message written to the rpm log including the level
  struct RpmLog {
    static constexpr std::string_view typeName = "RpmLog";
    uint32_t level;
    std::string line;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<RpmLog> fromStompMessage( const zypp::PluginFrame &msg );

  };

  // Per package information which directly correspond to a TransactionStep !!!!
  struct PackageBegin {
    static constexpr std::string_view typeName = "PackageBegin";
    uint32_t stepId;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<PackageBegin> fromStompMessage( const zypp::PluginFrame &msg );
  };

  struct PackageFinished {
    static constexpr std::string_view typeName = "PackageFinished";
    uint32_t stepId;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<PackageFinished> fromStompMessage( const zypp::PluginFrame &msg );
  };

  struct PackageError {
    static constexpr std::string_view typeName = "PackageError";
    uint32_t stepId;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<PackageError> fromStompMessage( const zypp::PluginFrame &msg );
  };

  struct PackageProgress {
    static constexpr std::string_view typeName = "PackageProgress";
    uint32_t stepId;
    uint32_t amount;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<PackageProgress> fromStompMessage( const zypp::PluginFrame &msg );
  };

  // Progress for cleaning up old versions of packages
  // which have NO corresponding TransactionStep
  struct CleanupBegin {
    static constexpr std::string_view typeName = "CleanupBegin";
    std::string nvra;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<CleanupBegin> fromStompMessage( const zypp::PluginFrame &msg );
  };

  struct CleanupFinished {
    static constexpr std::string_view typeName = "CleanupFinished";
    std::string nvra;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<CleanupFinished> fromStompMessage( const zypp::PluginFrame &msg );
  };

  struct CleanupProgress {
    static constexpr std::string_view typeName = "CleanupProgress";
    std::string nvra;
    uint32_t amount;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<CleanupProgress> fromStompMessage( const zypp::PluginFrame &msg );
  };

  // per script info, for each of those the stepId can be -1 or point
  // to a valid step in the transaction set.
  struct ScriptBegin {
    static constexpr std::string_view typeName = "ScriptBegin";
    int32_t stepId;
    std::string scriptType;
    std::string scriptPackage;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<ScriptBegin> fromStompMessage( const zypp::PluginFrame &msg );
  };

  struct ScriptFinished {
    static constexpr std::string_view typeName = "ScriptFinished";
    int32_t stepId;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<ScriptFinished> fromStompMessage( const zypp::PluginFrame &msg );
  };

  struct ScriptError {
    static constexpr std::string_view typeName = "ScriptError";
    int32_t stepId;
    bool   fatal;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<ScriptError> fromStompMessage( const zypp::PluginFrame &msg );
  };

  // Generic Transactionstep report, we use it for preparing and verifying progress
  struct TransBegin {
    static constexpr std::string_view typeName = "TransBegin";
    std::string name;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<TransBegin> fromStompMessage( const zypp::PluginFrame &msg );
  };

  struct TransFinished {
    static constexpr std::string_view typeName = "TransFinished";
    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<TransFinished> fromStompMessage( const zypp::PluginFrame &msg );
  };

  struct TransProgress {
    static constexpr std::string_view typeName = "TransProgress";
    uint32_t amount;

    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<TransProgress> fromStompMessage( const zypp::PluginFrame &msg );
  };

}

#endif // ZYPP_SHARED_COMMIT_COMMITMESSAGE_H_INCLUDED
