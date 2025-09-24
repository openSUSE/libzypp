#include <optional>
#include <vector>

#include <zypp-core/ng/core/ByteArray>
#include <zypp-core/ng/rpc/stompframestream.h>
#include <zypp-core/ng/base/private/linuxhelpers_p.h>
#include <zypp-core/AutoDispose.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/ShutdownLock_p.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/StringV.h>
#include <zypp-core/base/FileStreamBuf>

#include <zypp-core/rpc/PluginFrame.h>
#include <shared/commit/CommitMessages.h>

#include <boost/interprocess/sync/file_lock.hpp>
#include <mutex>

// we do not link against libzypp, but these are pure header only files, if that changes
// a copy should be created directly in the zypp-rpm project
#include <zypp/target/rpm/librpm.h>
#include <zypp/target/rpm/RpmFlags.h>

extern "C"
{
#include <rpm/rpmcli.h>
#include <rpm/rpmlog.h>
}

#include "BinHeader.h"
#include "errorcodes.h"

#include <cstdio>
#include <iostream>
#include <signal.h>
#include <unistd.h>

// Messages to the zypp log sent on stdout/stderr
#define ZDBG std::cout
#define ZERR std::cerr

namespace env
{
  /// Allows to increase rpm loglevel
  inline bool ZYPP_RPM_DEBUG()
  {
    static bool val = [](){
      const char * env = getenv("ZYPP_RPM_DEBUG");
      return( env && zypp::str::strToBool( env, true ) );
    }();
    return val;
  }
} // namespace env

namespace dbg
{
  struct _dumpTransactionStep
  {
    _dumpTransactionStep( const zypp::proto::target::TransactionStep *const step_r )
    : _step { step_r }
    {}
    std::ostream & dumpOn( std::ostream & str ) const
    {
      str << "TStep(";
      if ( _step ) {
        if ( std::holds_alternative<zypp::proto::target::InstallStep>(*_step) ) {
          const auto &step = std::get<zypp::proto::target::InstallStep>(*_step);
          str << step.stepId << "|" << step.pathname;
        } else if ( std::holds_alternative<zypp::proto::target::RemoveStep>(*_step) ) {
          const auto &step = std::get<zypp::proto::target::RemoveStep>(*_step);
          str << step.stepId << "|" << step.name << "-" << step.version << "-" << step.release << "." << step.arch;
        } else {
          str << "OOPS";
        }
      } else {
        str << "NULL";
      }
      return str << ")";
    }
    const zypp::proto::target::TransactionStep *const _step;
  };
  inline std::ostream & operator<<( std::ostream & str, const _dumpTransactionStep & obj )
  { return obj.dumpOn( str ); }
  inline _dumpTransactionStep dump( const zypp::proto::target::TransactionStep *const obj )
  { return _dumpTransactionStep( obj ); }
  inline _dumpTransactionStep dump( const zypp::proto::target::TransactionStep & obj )
  { return _dumpTransactionStep( &obj ); }

  struct _dumpRpmCallbackType
  {
    _dumpRpmCallbackType( rpmCallbackType_e flag_r )
    : _flag { flag_r }
    {}
    std::ostream & dumpOn( std::ostream & str ) const
    {
      str << "RPMCALLBACK";
      if ( _flag ) {
#define OUTS(E) if ( _flag & RPMCALLBACK_##E ) str << "_" << #E
        OUTS( INST_PROGRESS   );
        OUTS( INST_START      );
        OUTS( INST_OPEN_FILE  );
        OUTS( INST_CLOSE_FILE );
        OUTS( TRANS_PROGRESS  );
        OUTS( TRANS_START     );
        OUTS( TRANS_STOP      );
        OUTS( UNINST_PROGRESS );
        OUTS( UNINST_START    );
        OUTS( UNINST_STOP     );
        OUTS( UNPACK_ERROR    );
        OUTS( CPIO_ERROR      );
        OUTS( SCRIPT_ERROR    );
        OUTS( SCRIPT_START    );
        OUTS( SCRIPT_STOP     );
        OUTS( INST_STOP       );
        OUTS( ELEM_PROGRESS   );
#ifdef HAVE_RPM_VERIFY_TRANSACTION_STEP
        OUTS( VERIFY_PROGRESS );
        OUTS( VERIFY_START    );
        OUTS( VERIFY_STOP     );
#endif
#undef OUTS
      } else {
        str << "_UNKNOWN";
      }
      return str;
    }
    rpmCallbackType_e _flag;
  };
  inline std::ostream & operator<<( std::ostream & str, const _dumpRpmCallbackType & obj )
  { return obj.dumpOn( str ); }
  inline _dumpRpmCallbackType dump( rpmCallbackType obj )
  { return _dumpRpmCallbackType( obj ); }
}

// this is the order we expect the FDs we need to communicate to be set up
// by the parent. This is not pretty but it works and is less effort than
// setting up a Unix Domain Socket and sending FDs over that.
// Usually relying on conventions is not exactly a good idea but in this case we make an exception ;)
enum class ExpectedFds : int {
  MessageFd = STDERR_FILENO+1,
  ScriptFd  = STDERR_FILENO+2
};

using zypp::target::rpm::RpmInstFlag;
using zypp::target::rpm::RpmInstFlags;
using namespace zypprpm;

void *rpmLibCallback( const void *h, const rpmCallbackType what, const rpm_loff_t amount, const rpm_loff_t total, fnpyKey key, rpmCallbackData data );
int rpmLogCallback ( rpmlogRec rec, rpmlogCallbackData data );

template <typename Stream>
void rpmpsPrintToStream ( Stream &str, rpmps ps )
{
  if ( !ps )
    return;

  rpmProblem p = nullptr;
  zypp::AutoDispose<rpmpsi> psi ( ::rpmpsInitIterator(ps), ::rpmpsFreeIterator );
  while ((p = rpmpsiNext(psi))) {
    zypp::AutoFREE<char> msg( rpmProblemString(p) );
    str << "\t" << msg << std::endl;
  }
}

bool recvBytes ( int fd, char *buf, size_t n ) {
  size_t read = 0;
  while ( read != n ) {
    const auto r = zyppng::eintrSafeCall( ::read, fd, buf+read, n - read );
    if ( r <= 0 )
      return false;

    read += r;
  }
  return true;
}

bool sendBytes ( int fd, const void *buf, size_t n ) {
  const size_t written =  zyppng::eintrSafeCall( ::write, fd, buf, n );
  return written == n;
}

template <typename Message>
bool pushMessage ( const Message &msg ) {
  try {
    zypp::FdStreamBuf f;
    if ( !f.open( static_cast<int>( ExpectedFds::MessageFd ), std::ios_base::out ) )
      return false;

    // do not close the fd once the buffer is discarded
    f.disableAutoClose ();

    std::ostream outStr( &f );
    const auto &maybeMessage = msg.toStompMessage();
    if ( !maybeMessage ) {
      std::rethrow_exception ( maybeMessage.error() );
      return false;
    }

    maybeMessage->writeTo(outStr);
    return true;

  } catch ( const zypp::Exception &e ) {
    ZERR << "Failed to write message ("<<e<<")"<<std::endl;
  }
  return false;
}

bool pushTransactionErrorMessage ( rpmps ps )
{
  if ( !ps )
    return false;

  zypp::proto::target::TransactionError err;

  rpmProblem p = nullptr;
  zypp::AutoDispose<rpmpsi> psi ( ::rpmpsInitIterator(ps), ::rpmpsFreeIterator );
  while ((p = rpmpsiNext(psi))) {
    zypp::AutoFREE<char> msg( rpmProblemString(p) );
    err.problems.push_back( zypp::str::asString( msg.value() ) );
  }

  return pushMessage( err );
}


using RpmHeader = std::shared_ptr<std::remove_pointer_t<Header>>;
std::pair<RpmHeader, int> readPackage( rpmts ts_r, const zypp::filesystem::Pathname &path_r )
{
  zypp::PathInfo file( path_r );
  if ( ! file.isFile() ) {
    ZERR << "Not a file: " << path_r << std::endl;
    return std::make_pair( RpmHeader(), -1 );
  }

  FD_t fd = ::Fopen( path_r.c_str(), "r.ufdio" );
  if ( fd == 0  || ::Ferror(fd) )
  {
    ZERR << "Can't open file for reading: " << path_r << " (" << ::Fstrerror(fd) << ")" << std::endl;
    if ( fd )
      ::Fclose( fd );
    return std::make_pair( RpmHeader(), -1 );
  }

  Header nh = 0;
  int res = ::rpmReadPackageFile( ts_r, fd, path_r.asString().c_str(), &nh );
  ::Fclose( fd );

  if ( ! nh )
  {
    ZERR << "Error reading header from " << path_r << " error(" << res << ")" << std::endl;
    return std::make_pair( RpmHeader(), res );
  }

  RpmHeader h( nh, ::headerFree );
  return std::make_pair( h, res );
}

// Remember 'install source rpm' transaction steps (bsc#1228647, installed separately)
struct SourcePackageInstallHelper
{
  // < transaction step index, path to the .src.rpm >
  using StepIndex = std::pair<std::size_t,zypp::Pathname>;
  using StepIndices = std::vector<StepIndex>;

  void remember( std::size_t stepIndex_r, zypp::Pathname pathname_r )
  {
    if ( not stepIndices ) stepIndices = StepIndices();
    stepIndices->push_back( {stepIndex_r, std::move(pathname_r)} );
  }

  std::optional<StepIndices> stepIndices;   // Remembered source rpm installs
  std::optional<std::size_t> currentStep;   // Current transaction step index for rpmLibCallback
};

struct TransactionData {
  TransactionData( zypp::proto::target::Commit & commitData_r )
  : commitData { commitData_r }
  {}

  zypp::proto::target::Commit &commitData;

  // dbinstance of removals to transaction step index
  std::unordered_map<int, std::size_t> removePckIndex = {};

  // the fd used by rpm to dump script output
  zypp::AutoDispose<FD_t> rpmFd = {};

  // Remember 'install source rpm' transaction steps (bsc#1228647, installed separately)
  SourcePackageInstallHelper sourcePackageInstallHelper;
};

int main( int, char ** )
{

  // check if our env is set up correctly
  if ( ::isatty(STDIN_FILENO) || ::isatty(STDOUT_FILENO) || ::isatty(STDERR_FILENO) ) {
    ZERR << "Running zypp-rpm directly from the console is not supported. This is just a internal helper tool for libzypp." << std::endl;
    return OtherError;
  }

  // make sure the expected FDs are around too
  struct stat sb{};
  if ( fstat( static_cast<int>(ExpectedFds::MessageFd), &sb) == -1 ) {
    ZERR << "Expected message fd is not valid, aborting" << std::endl;
    return OtherError;
  }
  if ( (sb.st_mode & S_IFMT) != S_IFIFO ){
    ZERR << "Expected message fd is not a pipe, aborting" << std::endl;
    return OtherError;
  }

  if ( fstat( static_cast<int>(ExpectedFds::ScriptFd), &sb) == -1 ) {
    ZERR << "Expected script fd is not valid, aborting" << std::endl;
    return OtherError;
  }
  if ( (sb.st_mode & S_IFMT) != S_IFIFO ){
    ZERR << "Expected script fd is not a pipe, aborting" << std::endl;
    return OtherError;
  }

  // lets ignore those, SIGPIPE is better handled via the EPIPE error, and we do not want anyone
  // to CTRL+C us
  zyppng::blockSignalsForCurrentThread( { SIGPIPE, SIGINT } );

  // lets read our todo from stdin
  // since all we can receive on stdin is the commit message, there is no need to read a envelope first
  // we read it directly from the FD
  zypp::proto::target::Commit msg;

  try {
    zypp::PluginFrame pf( std::cin );
    const auto &expMsg = zypp::proto::target::Commit::fromStompMessage ( pf );
    if ( !expMsg ) {
      std::rethrow_exception ( expMsg.error() );
    }
    msg = std::move(*expMsg);
  } catch ( const zypp::Exception &e ) {
    ZERR << "Wrong commit message format, aborting (" << e << ")" << std::endl;
    return WrongMessageFormat;
  }

  // create or fill a pid file, if there is a existing one just take it over
  // if we reach this place libzypp has its global lock and made sure there is
  // no still running zypp-rpm instance. So no need to do anything complicated.
  using namespace boost::interprocess;
  struct s_lockinfo {
    s_lockinfo() = default;
    s_lockinfo( s_lockinfo && ) = default;
    s_lockinfo( const s_lockinfo & ) = delete;
    s_lockinfo &operator= ( s_lockinfo && ) = default;
    s_lockinfo &operator= ( const s_lockinfo & ) = delete;

    ~s_lockinfo() {
      std::scoped_lock<file_lock> lock(fileLock);
      clearerr( lockFile );
      ftruncate( fileno (lockFile), 0 );
      fflush( lockFile );
    }

    file_lock fileLock;
    zypp::AutoFILE lockFile;
  };
  std::optional<s_lockinfo> lockinfo;
  if ( !msg.lockFilePath.empty () ) {
    lockinfo.emplace();
    zypp::Pathname lockFileName = zypp::Pathname( msg.lockFilePath ) / "zypp-rpm.pid";
    lockinfo->lockFile = std::fopen( lockFileName.c_str(), "w");
    if ( lockinfo->lockFile == nullptr ) {
      ZERR << "Failed to create zypp-rpm pidfile." << std::endl;
      return FailedToCreateLock;
    }

    lockinfo->fileLock = file_lock ( lockFileName.c_str() );

    std::scoped_lock<file_lock> lock(lockinfo->fileLock);
    fprintf(lockinfo->lockFile, "%ld\n", (long)getpid() );
    fflush( lockinfo->lockFile );
  }

  zypp::ShutdownLockCommit lck("zypp-rpm");

  // we have all data ready now lets start installing
  // first we initialize the rpmdb
  int rc = ::rpmReadConfigFiles( NULL, NULL );
  if ( rc ) {
    ZERR << "rpmReadConfigFiles returned " << rc << std::endl;
    return RpmInitFailed;
  }

  ::addMacro( NULL, "_dbpath", NULL, msg.dbPath.c_str(), RMIL_CMDLINE );

  auto ts = zypp::AutoDispose<rpmts>( ::rpmtsCreate(), ::rpmtsFree );;
  ::rpmtsSetRootDir( ts, msg.root.c_str() );

  int tsFlags           = RPMTRANS_FLAG_NONE;
  int tsVerifyFlags     = RPMVSF_DEFAULT;

  const auto &rpmInstFlags = msg.flags;
  if ( rpmInstFlags & RpmInstFlag::RPMINST_NODIGEST)
    tsVerifyFlags |= _RPMVSF_NODIGESTS;
  if ( rpmInstFlags  & RpmInstFlag::RPMINST_NOSIGNATURE)
    tsVerifyFlags |= _RPMVSF_NOSIGNATURES;
  if ( rpmInstFlags  & RpmInstFlag::RPMINST_EXCLUDEDOCS)
    tsFlags |= RPMTRANS_FLAG_NODOCS;
  if ( rpmInstFlags  & RpmInstFlag::RPMINST_NOSCRIPTS)
    tsFlags |= RPMTRANS_FLAG_NOSCRIPTS;
  if ( rpmInstFlags  & RpmInstFlag::RPMINST_JUSTDB)
    tsFlags |= RPMTRANS_FLAG_JUSTDB;
  if ( rpmInstFlags  & RpmInstFlag::RPMINST_TEST)
    tsFlags |= RPMTRANS_FLAG_TEST;
  if ( rpmInstFlags  & RpmInstFlag::RPMINST_NOPOSTTRANS)
    tsFlags |= RPMTRANS_FLAG_NOPOSTTRANS;
  if ( rpmInstFlags & RpmInstFlag::RPMINST_NOSCRIPTS )
    tsFlags |= RPMTRANS_FLAG_NOSCRIPTS;

  // setup transaction settings
  ::rpmtsSetFlags( ts, tsFlags );

  // set the verify flags so readPackage does the right thing
  ::rpmtsSetVSFlags( ts, tsVerifyFlags );

#ifdef HAVE_RPMTSSETVFYLEVEL
  {
    int vfylevel = ::rpmtsVfyLevel(ts);
    if ( msg.flags() & RpmInstFlag::RPMINST_NODIGEST)
      vfylevel &= ~( RPMSIG_DIGEST_TYPE );
    if ( msg.flags()  & RpmInstFlag::RPMINST_NOSIGNATURE)
      vfylevel &= ~( RPMSIG_SIGNATURE_TYPE);
    ::rpmtsSetVfyLevel(ts, vfylevel);
  }
#endif

  // open database for reading
  if ( rpmtsGetRdb(ts) == NULL ) {
    int res = ::rpmtsOpenDB( ts, O_RDWR );
    if ( res ) {
      ZERR << "rpmdbOpen error(" << res << "): " << std::endl;
      return FailedToOpenDb;
    }
  }

  // the transaction data we will get in the callback
  TransactionData data { msg };

  // do we care about knowing the public key?
  const bool allowUntrusted = ( rpmInstFlags & RpmInstFlag::RPMINST_ALLOWUNTRUSTED );

  for ( std::size_t i = 0; i < msg.transactionSteps.size(); ++i ) {
    const auto &step = msg.transactionSteps[i];

    if ( std::holds_alternative<zypp::proto::target::InstallStep>(step) ) {

      const auto &install = std::get<zypp::proto::target::InstallStep>(step);

      const auto &file = install.pathname;
      auto rpmHeader = readPackage( ts, install.pathname );

      switch(rpmHeader.second) {
        case RPMRC_OK:
          break;
        case RPMRC_NOTTRUSTED:
          ZERR << zypp::str::Format( "Failed to verify key for %s" ) % file << std::endl;
          if ( !allowUntrusted )
            return FailedToReadPackage;
          break;
        case RPMRC_NOKEY:
          ZERR << zypp::str::Format( "Public key unavailable for %s" ) % file << std::endl;
          if ( !allowUntrusted )
            return FailedToReadPackage;
          break;
        case RPMRC_NOTFOUND:
          ZERR << zypp::str::Format( "Signature not found for %s" ) % file << std::endl;
          if ( !allowUntrusted )
            return FailedToReadPackage;
          break;
        case RPMRC_FAIL:
          ZERR << zypp::str::Format( "Signature does not verify for %s" ) % file << std::endl;
          return FailedToReadPackage;
        default:
          ZERR << zypp::str::Format( "Failed to open(generic error): %1%" ) % file << std::endl;
          return FailedToReadPackage;
      }

      if ( !rpmHeader.first ) {
        ZERR << zypp::str::Format( "Failed to read rpm header from: %1%" )% file << std::endl;
        return FailedToReadPackage;
      }

      // bsc#1228647: ::rpmtsInstallElement accepts source rpms without an error, but
      // is not able to handle them correctly within ::rpmtsRun. We need to remember
      // them and run ::rpmInstallSourcePackage after the ::rpmtsRun is done.
      if ( ::headerIsSource( rpmHeader.first.get() ) ) {
        data.sourcePackageInstallHelper.remember( i, install.pathname );
        continue;
      }

      const auto res = ::rpmtsAddInstallElement( ts, rpmHeader.first.get(), &step, !install.multiversion, nullptr  );
      if ( res ) {
        ZERR << zypp::str::Format( "Failed to add %1% to the transaction." )% file << std::endl;
        return FailedToAddStepToTransaction;
      }

    } else if ( std::holds_alternative<zypp::proto::target::RemoveStep>(step) ) {

      const auto &remove = std::get<zypp::proto::target::RemoveStep>(step);

      const std::string &name = remove.name
                                + "-" + remove.version
                                + "-" + remove.release
                                + "." + remove.arch;

      bool found = false;
      zypp::AutoDispose<rpmdbMatchIterator> it( ::rpmtsInitIterator( ts, rpmTag(RPMTAG_NAME), remove.name.c_str(), 0 ), ::rpmdbFreeIterator );
      while ( ::Header h = ::rpmdbNextIterator( it ) ) {
        BinHeader hdr(h);
        if ( hdr.string_val( RPMTAG_VERSION ) == remove.version
             &&  hdr.string_val( RPMTAG_RELEASE ) == remove.release
             &&  hdr.string_val( RPMTAG_ARCH ) == remove.arch ) {
          found = true;

          const auto res = ::rpmtsAddEraseElement( ts, hdr.get(), 0  );
          if ( res ) {
            ZERR << zypp::str::Format( "Failed to add removal of %1% to the transaction." ) % name << std::endl;
            return FailedToAddStepToTransaction;
          }

          data.removePckIndex.insert( std::make_pair( headerGetInstance( h ), i ) );
          break;
        }
      }

      if ( !found ) {
        ZERR << "Unable to remove " << name << " it was not found!" << std::endl;
      }

    } else {
      ZERR << "Ignoring step that is neither a remove, nor a install." << std::endl;
    }

  }

  // set the callback function for progress reporting and things
  ::rpmtsSetNotifyCallback( ts, rpmLibCallback, &data );

  // make sure we get da log
  ::rpmlogSetMask( RPMLOG_UPTO( RPMLOG_PRI(env::ZYPP_RPM_DEBUG() ? RPMLOG_DEBUG : RPMLOG_INFO) ) );
  ::rpmlogSetCallback( rpmLogCallback, nullptr );

  // redirect the script output to a fd ( log level MUST be at least INFO )
  data.rpmFd = zypp::AutoDispose<FD_t> (
    ::fdDup( static_cast<int>( ExpectedFds::ScriptFd ) ),
    Fclose
  );

  if ( data.rpmFd.value() ) {
    //ZDBG << "Assigning script FD" << std::endl;
    ::rpmtsSetScriptFd( ts,  data.rpmFd );
  } else {
    ZERR << "Failed to assign script FD" << std::endl;
  }


#if 0
  // unset the verify flags, we already checked those when reading the package header and libzypp made sure
  // all signatures are fine as well
  ::rpmtsSetVSFlags( ts, rpmtsVSFlags(ts) | _RPMVSF_NODIGESTS | _RPMVSF_NOSIGNATURES );

#ifdef HAVE_RPMTSSETVFYLEVEL
  {
    int vfylevel = ::rpmtsVfyLevel(ts);
    vfylevel &= ~( RPMSIG_DIGEST_TYPE | RPMSIG_SIGNATURE_TYPE);
    ::rpmtsSetVfyLevel(ts, vfylevel);
  }
#endif

#endif

  // handle --nodeps
  if ( !( msg.flags & RpmInstFlag::RPMINST_NODEPS) ) {
    if ( ::rpmtsCheck(ts) ) {
      zypp::AutoDispose<rpmps> ps( ::rpmtsProblems(ts), ::rpmpsFree );
      pushTransactionErrorMessage( ps );

      std::ostringstream sstr;
      sstr << "rpm output:" << "Failed dependencies:" << std::endl;
      rpmpsPrintToStream( sstr, ps );

      const auto &rpmMsg = sstr.str();

      // TranslatorExplanation the colon is followed by an error message
      ZERR << std::string("RPM failed: ") + rpmMsg << std::endl;
      return RpmFinishedWithTransactionError;
    }
  }

  // those two cases are already handled by libzypp at the time a package set arrives here,
  // we can safely filter those problems.
  int tsProbFilterFlags = RPMPROB_FILTER_REPLACEPKG | RPMPROB_FILTER_OLDPACKAGE;

  if ( msg.ignoreArch )
    tsProbFilterFlags |= RPMPROB_FILTER_IGNOREARCH;

  if ( msg.flags & RpmInstFlag::RPMINST_ALLOWDOWNGRADE )
    tsProbFilterFlags |= ( RPMPROB_FILTER_OLDPACKAGE ); // --oldpackage

  if ( msg.flags & RpmInstFlag::RPMINST_REPLACEFILES )
    tsProbFilterFlags |= ( RPMPROB_FILTER_REPLACENEWFILES
                           | RPMPROB_FILTER_REPLACEOLDFILES ); // --replacefiles

  if ( msg.flags & RpmInstFlag::RPMINST_FORCE )
    tsProbFilterFlags |= ( RPMPROB_FILTER_REPLACEPKG
                           | RPMPROB_FILTER_REPLACENEWFILES
                           | RPMPROB_FILTER_REPLACEOLDFILES
                           | RPMPROB_FILTER_OLDPACKAGE ); // --force

  if ( msg.flags & RpmInstFlag::RPMINST_IGNORESIZE )
    tsProbFilterFlags |= RPMPROB_FILTER_DISKSPACE | RPMPROB_FILTER_DISKNODES;

  const auto orderRes = rpmtsOrder( ts );
  if ( orderRes ) {
    ZERR << zypp::str::Format( "Failed with error %1% while ordering transaction." )% orderRes << std::endl;
    return RpmOrderFailed;
  }

  // clean up memory that is only used for dependency checks and ordering
  rpmtsClean(ts);

  // transaction steps are set up lets execute it
  // the way how libRPM works is that it will try to install all packages even if some of them fail
  // we need to go over the rpm problem set to mark those steps that have failed, we get no other hint on wether
  // it worked or not
  const auto transRes = ::rpmtsRun( ts, nullptr, tsProbFilterFlags );
  //data.finishCurrentStep( );

  if ( transRes != 0 ) {

    auto err = RpmFinishedWithError;

    std::string errMsg;
    if ( transRes > 0 ) {
      //@NOTE dnf checks if the problem set is empty and if it is seems to treat the transaction as successful, can this really happen?
      zypp::AutoDispose<rpmps> ps( ::rpmtsProblems(ts), ::rpmpsFree );

      pushTransactionErrorMessage( ps );

      std::ostringstream sstr;
      sstr << "rpm output:" << std::endl;
      rpmpsPrintToStream( sstr, ps );
      errMsg = sstr.str();
      err = RpmFinishedWithTransactionError;
    } else {
      errMsg = "Running the transaction failed.";
    }

    //HistoryLog().comment( str::form("Transaction failed"), true /*timestamp*/ );
    std::ostringstream sstr;
    sstr << "rpm output:" << std::endl << errMsg << std::endl;
    //HistoryLog().comment(sstr.str());

    ZERR << "RPM transaction failed: " + errMsg << std::endl;
    return err;
  }

  // bsc#1228647: ::rpmtsInstallElement accepts source rpms without an error, but
  // is not able to handle them correctly within ::rpmtsRun. We need to remember
  // them and run ::rpmInstallSourcePackage after the ::rpmtsRun is done.
  if ( data.sourcePackageInstallHelper.stepIndices ) {
    ::rpmtsEmpty( ts );   // Re-create an empty transaction set.
    auto err = NoError;
    for ( const auto & [stepi,path] : *data.sourcePackageInstallHelper.stepIndices ) {
      zypp::AutoDispose<FD_t> fd { ::Fopen( path.c_str(), "r" ), ::Fclose };
      data.sourcePackageInstallHelper.currentStep = stepi; // rpmLibCallback needs to look it up
      ::rpmRC res = ::rpmInstallSourcePackage( ts, fd, nullptr, nullptr );
      if ( res != 0 ) {
        ZERR << "RPM InstallSourcePackage " << path << " failed: " << res << std::endl;
        err = RpmFinishedWithError;
      }
    }
    data.sourcePackageInstallHelper.currentStep = std::nullopt; // done
    if ( err != NoError ) {
      return err;
    }
  }

  //ZDBG << "Success !!!!" << std::endl;

  return NoError;
}

std::string_view tagToScriptTypeName ( int tag )
{
  using namespace std::literals;
  switch (tag) {
    case RPMTAG_PREIN:
      return "prein"sv;
    case RPMTAG_PREUN:
      return "preun"sv;
    case RPMTAG_TRIGGERPREIN:
      return "triggerprein"sv;
    case RPMTAG_POSTIN:
      return "postin"sv;
    case RPMTAG_POSTUN:
      return "postun"sv;
    case RPMTAG_PRETRANS:
      return "pretrans"sv;
    case RPMTAG_POSTTRANS:
      return "posttrans"sv;
    case RPMTAG_TRIGGERUN:
      return "triggerun"sv;
    case RPMTAG_TRIGGERIN:
      return "triggerin"sv;
    case RPMTAG_TRIGGERPOSTUN:
      return "triggerpostun"sv;
    case RPMTAG_VERIFYSCRIPT:
      return "verifyscript"sv;
    default:
      return ""sv;
  }
}

void *rpmLibCallback( const void *h, const rpmCallbackType what, const rpm_loff_t amount, const rpm_loff_t total, fnpyKey key, rpmCallbackData data )
{
  void * rc = NULL;
  TransactionData* that = reinterpret_cast<TransactionData *>( data );
  if ( !that )
    return rc;

  static FD_t fd = NULL;
  const BinHeader header( (Header)h );

  auto iStep = key ? reinterpret_cast< const zypp::proto::target::TransactionStep * >( key ) : nullptr;
  if ( !iStep ) {
    if ( that->sourcePackageInstallHelper.currentStep ) {
      // we're in ::rpmInstallSourcePackage
      iStep = &that->commitData.transactionSteps[*(that->sourcePackageInstallHelper.currentStep)];
    }
    else if ( h ) {
      auto key = headerGetInstance( header.get() );
      if ( key > 0 ) {
        auto i = that->removePckIndex.find(key);
        if ( i != that->removePckIndex.end() )
          iStep = &that->commitData.transactionSteps[i->second];
      }
    }
  }

#if 0
  // dbg log callback sequence (collapsing RPMCALLBACK_INST_PROGRESS)
  static rpmCallbackType prev = RPMCALLBACK_UNKNOWN;
  if ( what != RPMCALLBACK_INST_PROGRESS || prev != RPMCALLBACK_INST_PROGRESS ) {
    ZDBG << "LibCB " << dbg::dump(what) << ": " << dbg::dump(iStep) << " - "  << header.nvra() << std::endl;
    prev = what;
  }
#endif

  const auto &sendEndOfScriptTag = [&](){
    //ZDBG << "Send end of script" << std::endl;
    ::Fflush( that->rpmFd );
    ::sendBytes( static_cast<int>( ExpectedFds::ScriptFd ), endOfScriptTag.data(), endOfScriptTag.size() );
  };

  switch (what) {
    case RPMCALLBACK_INST_OPEN_FILE: {

      if ( !iStep || !std::holds_alternative<zypp::proto::target::InstallStep>(*iStep) )
        return NULL;

      const auto &install = std::get<zypp::proto::target::InstallStep>(*iStep);
      if ( install.pathname.empty() )
          return NULL;

      if ( fd != NULL )
        ZERR << "ERR opening a file before closing the old one?  Really ? " << std::endl;
      fd = Fopen( install.pathname.data(), "r.ufdio" );
      if (fd == NULL || Ferror(fd)) {
        ZERR << "Error when opening file " << install.pathname.data() << std::endl;
        if (fd != NULL) {
          Fclose(fd);
          fd = NULL;
        }
      } else
        fd = fdLink(fd);
      return (void *)fd;
      break;
    }

    case RPMCALLBACK_INST_CLOSE_FILE:
      fd = fdFree(fd);
      if (fd != NULL) {
        Fclose(fd);
        fd = NULL;
      }
      break;

    case RPMCALLBACK_INST_START: {

      if ( !iStep || !std::holds_alternative<zypp::proto::target::InstallStep>(*iStep) )
        return rc;

      zypp::proto::target::PackageBegin step;
      step.stepId = std::get<zypp::proto::target::InstallStep>(*iStep).stepId;
      pushMessage( step );

      break;
    }

    case RPMCALLBACK_UNINST_START: {

      if ( !iStep ) {

        if ( header.empty() ) {
          ZERR << "No header and no transaction step for a uninstall start, not sending anything" << std::endl;
          return rc;
        }

        // this is a package cleanup send the report accordingly
        zypp::proto::target::CleanupBegin step;
        step.nvra = header.nvra();
        pushMessage( step );

      } else {

        if ( !std::holds_alternative<zypp::proto::target::RemoveStep>(*iStep) ) {
          ZERR << "Could not find package in removables " << header << " in transaction elements" << std::endl;
          return rc;
        }

        zypp::proto::target::PackageBegin step;
        step.stepId = std::get<zypp::proto::target::RemoveStep>(*iStep).stepId;
        pushMessage( step );
      }
      break;
    }

    case RPMCALLBACK_INST_STOP: {

      if ( !iStep ) {
        ZERR << "Could not find package " << header << " in transaction elements for " << dbg::dump(what) << std::endl;
        return rc;
      }

      sendEndOfScriptTag();

      zypp::proto::target::PackageFinished step;
      step.stepId = std::visit([](const auto &val){ return val.stepId;}, *iStep );
      pushMessage( step );

      break;
    }

    case RPMCALLBACK_UNINST_STOP: {

      if ( !iStep ) {
        if ( header.empty() ) {
          ZERR << "No header and no transaction step for a uninstall stop, not sending anything" << std::endl;
          return rc;
        }

        sendEndOfScriptTag();

        // this is a package cleanup send the report accordingly
        zypp::proto::target::CleanupFinished step;
        step.nvra = header.nvra();
        pushMessage( step );
      } else {

        sendEndOfScriptTag();

        zypp::proto::target::PackageFinished step;
        step.stepId = std::visit([](const auto &val){ return val.stepId;}, *iStep );
        pushMessage( step );

      }

      break;
    }
    case RPMCALLBACK_UNPACK_ERROR: {

      if ( !iStep ) {
        ZERR << "Could not find package " << header << " in transaction elements for " << dbg::dump(what) << std::endl;
        return rc;
      }

      zypp::proto::target::PackageError step;
      step.stepId = std::visit([](const auto &val){ return val.stepId;}, *iStep );
      pushMessage( step );
      break;
    }

    case RPMCALLBACK_INST_PROGRESS: {

      if ( !iStep  )
        return rc;

      const auto progress = (double) (total
                                        ? ((((float) amount) / total) * 100)
                                        : 100.0);

      zypp::proto::target::PackageProgress step;
      step.stepId = std::visit([](const auto &val){ return val.stepId;}, *iStep );
      step.amount = progress;
      pushMessage( step );

      break;
    }

    case RPMCALLBACK_UNINST_PROGRESS: {

      const auto progress = (double) (total
                                        ? ((((float) amount) / total) * 100)
                                        : 100.0);
      if ( !iStep  ) {
        if ( header.empty() ) {
          ZERR << "No header and no transaction step for a uninstall progress, not sending anything" << std::endl;
          return rc;
        }

        // this is a package cleanup send the report accordingly
        zypp::proto::target::CleanupProgress step;
        step.nvra = header.nvra();
        step.amount = progress;
        pushMessage( step );

      } else {
        zypp::proto::target::PackageProgress step;
        step.stepId = std::visit([](const auto &val){ return val.stepId;}, *iStep );
        step.amount = progress;
        pushMessage( step );
      }
      break;
    }

#ifdef HAVE_RPM_VERIFY_TRANSACTION_STEP
    case RPMCALLBACK_VERIFY_START:
#endif
    case RPMCALLBACK_TRANS_START: {
      zypp::proto::target::TransBegin step;
      const char *n = "Preparing";
#ifdef HAVE_RPM_VERIFY_TRANSACTION_STEP
      if ( what == RPMCALLBACK_VERIFY_START )
        n = "Verifying";
#endif
      step.name = n;
      pushMessage( step );
      break;
    }

#ifdef HAVE_RPM_VERIFY_TRANSACTION_STEP
    case RPMCALLBACK_VERIFY_STOP:
#endif
    case RPMCALLBACK_TRANS_STOP: {
      sendEndOfScriptTag();
      zypp::proto::target::TransFinished step;
      pushMessage( step );
      break;
    }

#ifdef HAVE_RPM_VERIFY_TRANSACTION_STEP
    case RPMCALLBACK_VERIFY_PROGRESS:
#endif
    case RPMCALLBACK_TRANS_PROGRESS: {
      const auto percentage = (double) (total
                                      ? ((((float) amount) / total) * 100)
                                      : 100.0);
      zypp::proto::target::TransProgress prog;
      prog.amount = percentage;
      pushMessage( prog );
      break;
    }
    case RPMCALLBACK_CPIO_ERROR:
      ZERR << "CPIO Error when installing package" << std::endl;
      break;
    case RPMCALLBACK_SCRIPT_START: {
      zypp::proto::target::ScriptBegin script;
      if ( iStep )
        script.stepId = std::visit([](const auto &val){ return val.stepId;}, *iStep );
      else
        script.stepId = -1;

      // script type is stored in the amount variable passed to the callback
      script.scriptType = std::string( tagToScriptTypeName( amount ) );

      if ( header.get() ) {
        script.scriptPackage = header.nvra();
      }

      pushMessage( script );
      break;
    }

    case RPMCALLBACK_SCRIPT_STOP: {
      sendEndOfScriptTag();
      zypp::proto::target::ScriptFinished script;
      if ( iStep )
        script.stepId = std::visit([](const auto &val){ return val.stepId;}, *iStep );
      else
        script.stepId = ( -1 );
      pushMessage( script );
      break;
    }
    case RPMCALLBACK_SCRIPT_ERROR: {
      zypp::proto::target::ScriptError script;
      if ( iStep )
        script.stepId = std::visit([](const auto &val){ return val.stepId;}, *iStep );
      else
        script.stepId = ( -1 );

      // for RPMCALLBACK_SCRIPT_ERROR 'total' is abused by librpm to distinguish between warning and "real" errors
      script.fatal = ( total != RPMRC_OK );
      pushMessage( script );
      break;
    }
    case RPMCALLBACK_UNKNOWN:
    default:
      break;
  }

  return rc;
}

int rpmLogCallback ( rpmlogRec rec, rpmlogCallbackData )
{
  int logRc = 0;

  zypp::proto::target::RpmLog log;
  log.level = rpmlogRecPriority(rec);
  log.line  =  zypp::str::asString( ::rpmlogRecMessage(rec) );
  pushMessage( log );

  return logRc;
}
