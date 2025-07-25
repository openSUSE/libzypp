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
#include <utility>
#include <zypp-core/ng/pipelines/Expected>
#include <zypp-media/FileCheckException>
#include <zypp-media/ng/Provide>
#include <zypp/Digest.h>
#include <zypp/ng/Context>
#include <zypp/ng/reporthelper.h>
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

    CheckSumWorkflowLogic( ZyppContextRefType zyppContext, zypp::CheckSum &&checksum, zypp::Pathname file )
      : _context( std::move(zyppContext) )
      , _report( _context )
      , _checksum(std::move( checksum ))
      , _file(std::move( file ))
      {}

    auto execute()
    {

      //MIL << "checking " << file << " file against checksum '" << _checksum << "'" << endl;
      if ( _checksum.empty() ) {
        MIL << "File " <<  _file << " has no checksum available." << std::endl;
        if ( _report.askUserToAcceptNoDigest( _file ) ) {
          MIL << "User accepted " <<  _file << " with no checksum." << std::endl;
          return makeReadyResult( expected<void>::success() );
        } else {
          return makeReadyResult( expected<void>::error( ZYPP_EXCPT_PTR(zypp::FileCheckException( _file.basename() + " has no checksum" ) ) ) );
        }

      } else {

        return _context->provider()->checksumForFile ( _file, _checksum.type() )
          | [] ( expected<zypp::CheckSum> sum ) {
            if ( !sum )
              return zypp::CheckSum( );
            return sum.get();
          }
          | [this, _file=_file]( zypp::CheckSum real_checksum ){
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
               else if ( _report.askUserToAcceptWrongDigest( _file, _checksum.checksum(), real_checksum.checksum() ) )
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
    DigestReportHelper<ZyppContextRefType> _report;
    zypp::CheckSum _checksum;
    zypp::Pathname _file;

  };

  expected<void> verifyChecksum( SyncContextRef zyppCtx, zypp::CheckSum checksum, zypp::Pathname file )
  {
    return SimpleExecutor<CheckSumWorkflowLogic, SyncOp<expected<void>>>::run( std::move(zyppCtx), std::move(checksum), std::move(file) );
  }

  AsyncOpRef<expected<void> > verifyChecksum( ContextRef zyppCtx, zypp::CheckSum checksum, zypp::filesystem::Pathname file )
  {
    return SimpleExecutor<CheckSumWorkflowLogic, AsyncOp<expected<void>>>::run( std::move(zyppCtx), std::move(checksum), std::move(file) );
  }

  std::function<AsyncOpRef<expected<ProvideRes> > (ProvideRes &&)> checksumFileChecker( ContextRef zyppCtx, zypp::CheckSum checksum )
  {
    using zyppng::operators::operator|;
    return [ zyppCtx, checksum=std::move(checksum) ]( ProvideRes res ) mutable -> AsyncOpRef<expected<ProvideRes>> {
      return verifyChecksum( zyppCtx, std::move(checksum), res.file() )
       | [ res ] ( expected<void> result ) mutable {
          if ( result )
            return expected<ProvideRes>::success( std::move(res) );
          else
            return expected<ProvideRes>::error( std::move(result.error()) );
      };
    };
  }

  std::function<expected<SyncProvideRes> (SyncProvideRes &&)> checksumFileChecker(SyncContextRef zyppCtx, zypp::CheckSum checksum)
  {
    using zyppng::operators::operator|;
    return [ zyppCtx = std::move(zyppCtx), checksum=std::move(checksum) ]( SyncProvideRes res ) mutable -> expected<SyncProvideRes> {
      return verifyChecksum( zyppCtx, std::move(checksum), res.file() )
       | [ res ] ( expected<void> result ) mutable {
          if ( result )
            return expected<SyncProvideRes>::success( std::move(res) );
          else
            return expected<SyncProvideRes>::error( std::move(result.error()) );
      };
    };
  }

}
