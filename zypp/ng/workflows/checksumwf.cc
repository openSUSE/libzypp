/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "checksumwf.h"
#include "logichelpers.h"

#include <zypp-core/Date.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/CheckSum.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-media/FileCheckException>
#include <zypp-media/ng/Provide>
#include <zypp/Digest.h>
#include <zypp/ng/Context>
#include <zypp/ng/UserRequest>

#include <map>

namespace zyppng::CheckSumWorkflow {

  using namespace zyppng::operators;

  template <class Executor, class OpType >
  struct CheckSumWorkflowLogic : public LogicBase<Executor, OpType>
  {
    ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

    using ZyppContextRefType = MaybeAsyncContextRef<OpType>;
    using ZyppContextType = remove_smart_ptr_t<ZyppContextRefType>;
    using ProvideType     = typename ZyppContextType::ProvideType;
    using MediaHandle     = typename ProvideType::MediaHandle;
    using ProvideRes      = typename ProvideType::Res;

    CheckSumWorkflowLogic( ZyppContextRefType zyppContext, const zypp::CheckSum &checksum, const zypp::Pathname &file )
      : _context( std::move(zyppContext) )
      , _checksum( checksum )
      , _file( file )
      {}

    auto execute()
    {

      //MIL << "checking " << file << " file against checksum '" << _checksum << "'" << endl;
      if ( _checksum.empty() ) {
        MIL << "File " <<  _file << " has no checksum available." << std::endl;
        if ( executor()->askUserToAcceptNoDigest( _file ) ) {
          MIL << "User accepted " <<  _file << " with no checksum." << std::endl;
          return makeReadyResult( expected<void>::success() );
        } else {
          return makeReadyResult( expected<void>::error( ZYPP_EXCPT_PTR(zypp::FileCheckException( _file.basename() + " has no checksum" ) ) ) );
        }

      } else {

        return _context->provider()->checksumForFile ( _file, _checksum.type() )
          | [] ( expected<zypp::CheckSum> &&sum ) {
            if ( !sum )
              return zypp::CheckSum( );
            return sum.get();
          }
          | [this, _file=_file]( zypp::CheckSum &&real_checksum ){
             if ( (real_checksum != _checksum) )
             {
               // Remember askUserToAcceptWrongDigest decision for at most 12hrs in memory;
               // Actually we just want to prevent asking the same question again when the
               // previously downloaded file is retrieved from the disk cache.
               static std::map<std::string,std::string> exceptions;
               static zypp::Date exceptionsAge;
               zypp::Date now( zypp::Date::now() );
               if ( !exceptions.empty() && now-exceptionsAge > 12*zypp::Date::hour )
                 exceptions.clear();

               WAR << "File " <<  _file << " has wrong checksum " << real_checksum << " (expected " << _checksum << ")" << std::endl;
               if ( !exceptions.empty() && exceptions[real_checksum.checksum()] == _checksum.checksum() )
               {
                 WAR << "User accepted " <<  _file << " with WRONG CHECKSUM. (remembered)" << std::endl;
                 return expected<void>::success();
               }
               else if ( executor()->askUserToAcceptWrongDigest( _file, _checksum.checksum(), real_checksum.checksum() ) )
               {
                 WAR << "User accepted " <<  _file << " with WRONG CHECKSUM." << std::endl;
                 exceptions[real_checksum.checksum()] = _checksum.checksum();
                 exceptionsAge = now;
                 return expected<void>::success();
               }
               else
               {
                 return expected<void>::error( ZYPP_EXCPT_PTR(zypp::FileCheckException(  _file.basename() + " has wrong checksum" ) ) );
               }
             }
             return expected<void>::success();
          };
      }
    }

  protected:
    ZyppContextRefType _context;
    zypp::CheckSum _checksum;
    zypp::Pathname _file;

  };

  struct AsyncCheckSumExecutor : public CheckSumWorkflowLogic< AsyncCheckSumExecutor, AsyncOp<expected<void>> >
  {
    using CheckSumWorkflowLogic< AsyncCheckSumExecutor, AsyncOp<expected<void>> >::CheckSumWorkflowLogic;

    bool askUserToAcceptNoDigest ( const zypp::Pathname &file ) {
      const auto &label = (zypp::str::Format(_("No digest for file %s.")) % file ).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptNoDigestRequest::makeData(file) );
      _context->sendUserRequest( req );
      return req->choice ();
    }
    bool askUserToAccepUnknownDigest ( const zypp::Pathname &file, const std::string &name ) {
      const auto &label = (zypp::str::Format(_("Unknown digest %s for file %s.")) %name % file).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptUnknownDigestRequest::makeData(file, name) );
      _context->sendUserRequest( req );
      return req->choice ();
    }
    bool askUserToAcceptWrongDigest ( const zypp::Pathname &file, const std::string &requested, const std::string &found ) {
      const auto &label = (zypp::str::Format(_("Digest verification failed for file '%s'")) % file).str();
      auto req = BooleanChoiceRequest::create( label, false, AcceptWrongDigestRequest::makeData(file, requested, found) );
      _context->sendUserRequest( req );
      return req->choice ();
    }

  };

  struct SyncCheckSumExecutor : public CheckSumWorkflowLogic< SyncCheckSumExecutor, SyncOp<expected<void>> >
  {
    using CheckSumWorkflowLogic< SyncCheckSumExecutor, SyncOp<expected<void>> >::CheckSumWorkflowLogic;

    zypp::CheckSum checksumFromFile ( SyncContextRef &, const zypp::Pathname &path, const std::string &checksumType ) {
      return zypp::CheckSum( checksumType, zypp::filesystem::checksum( path, checksumType ));
    }
    bool askUserToAcceptNoDigest ( const zypp::Pathname &file ) {
      zypp::callback::SendReport<zypp::DigestReport> report;
      return report->askUserToAcceptNoDigest(file);
    }
    bool askUserToAccepUnknownDigest ( const zypp::Pathname &file, const std::string &name ) {
      zypp::callback::SendReport<zypp::DigestReport> report;
      return report->askUserToAccepUnknownDigest( file, name );
    }
    bool askUserToAcceptWrongDigest ( const zypp::Pathname &file, const std::string &requested, const std::string &found ) {
      zypp::callback::SendReport<zypp::DigestReport> report;
      return report->askUserToAcceptWrongDigest( file, requested, found );
    };
  };

  expected<void> verifyChecksum( SyncContextRef zyppCtx, const zypp::CheckSum &checksum, const zypp::Pathname &file )
  {
    return SyncCheckSumExecutor::run( std::move(zyppCtx), checksum, file );
  }

  AsyncOpRef<expected<void> > verifyChecksum( ContextRef zyppCtx, const zypp::CheckSum &checksum, const zypp::filesystem::Pathname &file )
  {
    return AsyncCheckSumExecutor::run( std::move(zyppCtx), checksum, file );
  }

  std::function<AsyncOpRef<expected<ProvideRes> > (ProvideRes &&)> checksumFileChecker( ContextRef zyppCtx, const zypp::CheckSum &checksum )
  {
    using zyppng::operators::operator|;
    return [ zyppCtx, checksum=checksum ]( ProvideRes &&res ) -> AsyncOpRef<expected<ProvideRes>> {
      return verifyChecksum( zyppCtx, checksum, res.file() )
       | [ res = std::move(res)] ( expected<void> &&result ) mutable {
          if ( result )
            return expected<ProvideRes>::success( std::move(res) );
          else
            return expected<ProvideRes>::error( std::move(result.error()) );
      };
    };
  }

  std::function<expected<SyncProvideRes> (SyncProvideRes &&)> checksumFileChecker(SyncContextRef zyppCtx, const zypp::CheckSum &checksum)
  {
    using zyppng::operators::operator|;
    return [ zyppCtx = std::move(zyppCtx), checksum=checksum ]( SyncProvideRes &&res ) -> expected<SyncProvideRes> {
      return verifyChecksum( zyppCtx, checksum, res.file() )
       | [ res = std::move(res)] ( expected<void> &&result ) mutable {
          if ( result )
            return expected<SyncProvideRes>::success( std::move(res) );
          else
            return expected<SyncProvideRes>::error( std::move(result.error()) );
      };
    };
  }

}
