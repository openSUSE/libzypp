#include <zypp/zyppng/monadic/Expected>
#include <zypp/zyppng/monadic/AsyncResult>
#include <zypp/zyppng/monadic/Transform>
#include <zypp/zyppng/monadic/MTry>
#include <zypp/zyppng/monadic/Wait>
#include <zypp/zyppng/monadic/Await>
#include <zypp/zyppng/monadic/Lift>
#include <zypp/zyppng/monadic/Redo>
#include <zypp/zyppng/base/Timer>

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/assign.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/as_array.hpp>
#include <boost/range/irange.hpp>
#include <boost/utility.hpp>

#include <zypp/zyppng/media/network/networkrequestdispatcher.h>
#include <zypp/zyppng/media/network/request.h>
#include <zypp/zyppng/base/Signals>
#include <zypp/zyppng/base/EventDispatcher>
#include <zypp/zyppng/core/Url>
#include <zypp/Pathname.h>
#include <zypp/CheckSum.h>
#include <zypp/AutoDispose.h>
#include <zypp/media/MetaLinkParser.h>
#include <zypp/base/Logger.h>

#include <exception>
#include <iostream>
#include <boost/optional.hpp>
#include <fstream>
#include <stddef.h>

namespace zyppng {
  std::vector<char> peek_data_fd ( FILE *fd, off_t offset, size_t count );
}

struct DlRequest {
  zyppng::Url url;
  zypp::Pathname path;
  std::string checksum;
  zypp::Pathname templateFile = zypp::Pathname();
};

auto makeData ()
{
  return std::vector<DlRequest> {
    DlRequest{
      zyppng::Url( "http://download.opensuse.org/distribution/leap/15.0/repo/oss/x86_64/0ad-0.0.22-lp150.2.10.x86_64.rpm")
        , zypp::Pathname("/tmp/test/0ad-0.0.22-lp150.2.10.x86_64.rpm")
        , "11822f1421ae50fb1a07f72220b79000"
    }
    , DlRequest{
      zyppng::Url( "http://download.opensuse.org/distribution/leap/15.0/repo/oss/x86_64/alsa-1.1.5-lp150.4.3.x86_64.rpm")
        , zypp::Pathname("/tmp/test/alsa-1.1.5-lp150.4.3.x86_64.rpm")
        , "b0aaaca4c3763792a495de293c8431c5"
    }
    , DlRequest{
      zyppng::Url( "http://download.opensuse.org/distribution/leap/15.0/repo/oss/x86_64/amarok-2.9.0-lp150.2.1.x86_64.rpm")
        , zypp::Pathname("/tmp/test/amarok-2.9.0-lp150.2.1.x86_64.rpm")
        , "a6eb92351c03bcf603a09a2e8eddcead"
    }
  };
}

bool looks_like_metalink_data( const std::vector<char> &data )
{
  if ( data.empty() )
    return false;

  const char *p = data.data();
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
    p++;

  if (!strncasecmp(p, "<?xml", 5))
  {
    while (*p && *p != '>')
      p++;
    if (*p == '>')
      p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
      p++;
  }
  bool ret = !strncasecmp( p, "<metalink", 9 ) ? true : false;
  return ret;
}

bool looks_like_metalink_file( const zypp::Pathname &file )
{
  std::unique_ptr<FILE, decltype(&fclose)> fd( fopen( file.c_str(), "r" ), &fclose );
  if ( !fd )
    return false;
  return looks_like_metalink_data( zyppng::peek_data_fd( fd.get(), 0, 256 ) );
}


//Demo User interface, shows how we could interact with the console/output
struct UserInterface
{
  uint newProgress ( std::string &&txt ) {
    uint progress = nextProgressId++;
    std::cout << "Starting progress " << progress << " " <<txt<<std::endl;
    return progress;
  }
  void progressAlive ( uint progressId, std::string &&txt ) {
    std::cout << time(nullptr) << "Progress " << progressId << " " << txt << " alive callback" << std::endl;
  }
  void progressUpdate ( uint progressId, std::string &&txt, uint currentProgress, uint maxProgress ) {
    std::cout << time(nullptr) << "Progress " << progressId << " " << txt << " " << currentProgress << "/" << maxProgress << std::endl;
  }
  void progressDone ( uint progressId, std::string &&txt ) {
    std::cout << time(nullptr) << "Progress " << progressId << " " << txt << " is done." << std::endl;
  }

private:
  uint nextProgressId = 0;
};


//Demo Context, this should be the central object that contains the state of
//libzypp. Instead of using a global instance this one is passed down to the code to allow
//multiple different context versions
struct ZyppContext
{

  ZyppContext() {
    downloader->run();
  }

  std::unique_ptr<UserInterface> userInterface = std::make_unique<UserInterface>();
  zyppng::EventDispatcher::Ptr ev = zyppng::EventDispatcher::createMain();
  zyppng::NetworkRequestDispatcher::Ptr downloader = std::make_shared<zyppng::NetworkRequestDispatcher>();
};


//tracks the download of a file that might be a metalink
//uses a functor to track the state
auto trackPossibleMetalinkFile ( std::shared_ptr<ZyppContext> ctx, zyppng::NetworkRequest::Ptr &&ptr ) {
  using namespace zyppng::operators;

  struct dltracker {
    dltracker( uint id, std::shared_ptr<ZyppContext> ctx ) : _id(id), _ctx ( std::move(ctx) ) {}

    void operator() (  zyppng::NetworkRequest &req, off_t dlTotal, off_t dlnow, off_t, off_t  ) {

      if ( dlnow >= testLen && _state == Untested ) {
        if( req.peekData(0, testLen) | looks_like_metalink_data ) {
          _state = Metalink;
        } else {
          _state = NoMetalink;
        }
      }

      switch ( _state ) {
        case Untested:
        case Metalink:
          _ctx->userInterface->progressAlive( _id, req.url().asString() );
          break;
        case NoMetalink:
          _ctx->userInterface->progressUpdate( _id, req.url().asString(), dlnow, dlTotal);
          break;
      }
    }

  private:

    enum {
     Untested,
     Metalink,
     NoMetalink
    } _state = Untested;

    const uint _id;
    const size_t testLen = 265;
    std::shared_ptr<ZyppContext> _ctx;
  };

  auto progId = ctx->userInterface->newProgress( ptr->url().asString() );
  ptr->sigProgress().connect( dltracker( progId , ctx) );
  ptr->sigFinished().connect( [ progId, ctx ]( zyppng::NetworkRequest &req, const zyppng::NetworkRequestError & ){
      ctx->userInterface->progressDone( progId, req.url().asString());
  });
  return std::move(ptr);
}

//small helper tool to migrate a boost range into a vector
struct to_vector_holder
{ };

namespace  {
to_vector_holder to_vector() {
  return {};
}
}

template< class SinglePassRange >
std::vector< typename decltype ( boost::begin( std::declval<SinglePassRange>() ) )::value_type > operator| ( SinglePassRange&& r, to_vector_holder&& )
{
  std::vector< typename decltype ( boost::begin(r) )::value_type > vec;
  boost::copy( std::forward<SinglePassRange>(r), std::back_inserter(vec));
  return vec;
}

//function that asserts that a given file has the expected checksum, if not it throws a exception
void checksumFile ( const zypp::Pathname &path, const std::string &expectedSum )
{
  std::ifstream in(path.asString() ,std::ios::binary);
  if ( !in.is_open() )
    throw zypp::Exception("Unknown File expception");

  zypp::CheckSum sum = zypp::CheckSum::md5( in );
  if ( sum.checksum() != expectedSum ) {
    std::cout << sum.checksum() << " is not " << expectedSum <<std::endl;
    throw zypp::Exception("Invalid checksum expception");
  } else {
    std::cout << "File " << path << " has valid checksum." <<std::endl;
  }
}

//lifts checksumFile to be used with a DlRequest
DlRequest checksumFromDLRequest ( const DlRequest &req ) {
  checksumFile( req.path, req.checksum );
  return req;
}

//tools to handle a metalink file
struct MetaLinkFile {

  using Ptr = std::shared_ptr<MetaLinkFile>;

  zypp::media::MediaBlockList blocks;
  std::vector<zypp::Url> mirrors;

  static zyppng::expected<MetaLinkFile::Ptr> parse ( const zypp::Pathname & path) {
    zypp::media::MetaLinkParser parser;
    try {
      parser.parse(path);
    } catch ( const std::exception &ex ) {
      return zyppng::expected<MetaLinkFile::Ptr>::error( std::make_exception_ptr(ex) );
    }

    MetaLinkFile::Ptr f = std::make_shared<MetaLinkFile>();
    f->blocks = parser.getBlockList();
    f->mirrors = parser.getUrls();

    return zyppng::expected<MetaLinkFile::Ptr>::success( std::move(f) );
  }

  static zyppng::expected<MetaLinkFile::Ptr> generateBlockList ( MetaLinkFile::Ptr &&mlFile ) {
    if ( mlFile->blocks.haveBlocks() )
      return zyppng::expected<MetaLinkFile::Ptr>::success( std::move(mlFile) );

    std::cout << "Generate blocklist, since there was none in the metalink file." << std::endl;
    const off_t BLKSIZE = 131072;

    off_t currOff = 0;
    off_t filesize = mlFile->blocks.getFilesize();
    while ( currOff <  filesize )  {

      auto blksize = static_cast<size_t>( filesize - currOff );
      if ( blksize > BLKSIZE)
        blksize = BLKSIZE;

      mlFile->blocks.addBlock( currOff, blksize );
      currOff += blksize;
    }

    std::cout << "Generated blocklist: " << std::endl << mlFile->blocks << std::endl << " End blocklist " << std::endl;
    return zyppng::expected<MetaLinkFile::Ptr>::success( std::move(mlFile) );
  }
};

class DownloadErrorException : public zypp::Exception
{
public:
  DownloadErrorException( const zyppng::NetworkRequestError &err ) : zypp::Exception ( err.toString() ), myError( err) {}
  zyppng::NetworkRequestError myError;
};

//Information required to download one block of a file
struct BlockRequest {

  size_t blockNr;
  zypp::media::MediaBlock block;
  zypp::Pathname path;
  zyppng::Url  originalUrl;  //< The unstripped URL as it was passed to Download , before transfer settings are removed

  std::shared_ptr<zypp::Digest> digest = std::shared_ptr<zypp::Digest>();
  std::vector<unsigned char> expectedChecksum = std::vector<unsigned char>();

  int  retryCount = 0;
  bool triedCredFromStore = false; //< already tried to authenticate from credential store?

  static std::shared_ptr<BlockRequest> make ( DlRequest initialReq, const MetaLinkFile::Ptr &file, size_t blockNr ) {

    auto blocks = file->blocks;

    auto req = std::make_shared<BlockRequest>( BlockRequest {
      blockNr,
      file->blocks.getBlock( blockNr ),
      std::move( initialReq.path ),
      std::move( initialReq.url )
    });

    if ( blocks.haveChecksum(blockNr) ) {
      req->digest = std::make_shared<zypp::Digest>();
      blocks.createDigest( *req->digest );
      req->expectedChecksum = blocks.getChecksum( blockNr );
    }

    return req;
  }

  static auto download ( std::shared_ptr<ZyppContext> ctx, std::shared_ptr<BlockRequest> req ) {
    using namespace zyppng;
    using namespace zyppng::operators;

    req->retryCount++;

    return std::make_shared<zyppng::NetworkRequest>( req->originalUrl, req->path, req->block.off, req->block.size, NetworkRequest::WriteShared  )
             | [ &req ]( auto && nwReq) {
                 nwReq->setPriority( NetworkRequest::High );
                 if ( req->digest ) {
                   nwReq->setDigest( req->digest );
                   nwReq->setExpectedChecksum( req->expectedChecksum );
                   std::cout << time(nullptr) << " Starting block  " << req->blockNr << " with checksum " << zypp::Digest::digestVectorToString( req->expectedChecksum ) << std::endl;
                 }
                 return std::forward<decltype (nwReq)>(nwReq);
               }
             | [ctx]( auto &&req ) { ctx->downloader->enqueue( req );  return std::forward<decltype (req)>(req); }
             | await<zyppng::NetworkRequest::Ptr>( &zyppng::NetworkRequest::sigFinished )
             | [ blockReq = req ]( auto &&req ) -> zyppng::expected<std::shared_ptr<BlockRequest>> {
                if ( req->hasError() ) {
                  std::cout << time(nullptr) << " Finished block  " << blockReq->blockNr << " with error " << std::endl;
                  return zyppng::expected<std::shared_ptr<BlockRequest>>::error( std::make_exception_ptr( DownloadErrorException( req->error() ) ) );
                } else {
                  std::cout << time(nullptr) << " Successfully Finished block  " << blockReq->blockNr << std::endl;
                  return zyppng::expected<std::shared_ptr<BlockRequest>>::success( blockReq );
                }
            }
    ;
  }
};

int main ( int, char *[] )
{
  using namespace zyppng;
  using namespace zyppng::operators;
  using namespace boost::adaptors;

  std::shared_ptr<ZyppContext> ctx = std::make_shared<ZyppContext>();

  auto makeDownloadRequest = [ ctx ] ( const DlRequest &reqParm ) {
    zyppng::NetworkRequest::Ptr req = std::make_shared<zyppng::NetworkRequest>( reqParm.url, reqParm.path );

    //forward the request pointer, but still remember the DlRequest
    return std::make_pair( req, reqParm );
  };

  auto res = makeData()
    | transformed ( [&makeDownloadRequest, ctx]( const DlRequest &parm ) {
        return makeDownloadRequest(parm)
               //enable the download to receive a multipart DL
               | []( auto &&req ) {
                  req.first->transferSettings().addHeader("Accept: */*, application/metalink+xml, application/metalink4+xml");
                  return std::forward<decltype (req)>(req);
                 }
               //enable tracking in the UI
               | lift( std::bind<zyppng::NetworkRequest::Ptr>( trackPossibleMetalinkFile, ctx, std::placeholders::_1 ) )
               //enqueue the download into the dispatcher
               | lift( [ctx]( auto &&req ) { ctx->downloader->enqueue( req );  return std::forward<decltype(req)>(req); } )
               //wait for dl to be ready
               | lift( await<zyppng::NetworkRequest::Ptr>( &zyppng::NetworkRequest::sigFinished ) )
               | []( auto &&req ) {
                  if ( req.first->hasError() ) {
                    return zyppng::expected<DlRequest>::error( std::make_exception_ptr( zypp::Exception( req.first->error().toString() ) ) );
                  }
                  return zyppng::expected<DlRequest>::success( std::move(req.second) );
                }

               //at this point we have either the file downloaded or a metalink description file
               | [ ctx ]( auto &&expectedReq ) -> AsyncOpPtr<zyppng::expected<DlRequest>> {
                   if ( !expectedReq )
                     return zyppng::makeReadyResult( zyppng::expected<DlRequest>::error( expectedReq.error() ) );

                   auto req = std::move(expectedReq.get());

                   if ( looks_like_metalink_file( req.path ) ) {

                     using namespace zyppng::operators;

                     auto res = MetaLinkFile::parse( req.path )
                                | mbind( MetaLinkFile::generateBlockList )
                                | mbind( [ ctx, req ]( MetaLinkFile::Ptr &&metalinkFile ) -> AsyncOpPtr<zyppng::expected<DlRequest>> {

                                    auto makeBlockRequest = std::bind( &BlockRequest::make, req, metalinkFile, std::placeholders::_1 );
                                    
                                    auto blockRequests =
                                      boost::irange<size_t>(0, metalinkFile->blocks.numBlocks() )
                                      | transformed( makeBlockRequest )
                                      | transformed( [ ctx ]( const std::shared_ptr<BlockRequest> &blkReq ){

                                          return std::shared_ptr<BlockRequest>(blkReq)
                                                 | redo_while(
                                                     [ctx]( std::shared_ptr<BlockRequest> &&req ) {
                                                       return BlockRequest::download( ctx, req );
                                                     },
                                                     []( zyppng::expected<std::shared_ptr<BlockRequest>> &res  ){
                                                       try {
                                                         if ( res )
                                                           return false; //stop the loop we are good
                                                         std::rethrow_exception( res.error() );
                                                        } catch ( DownloadErrorException &err ) {
                                                         if ( err.myError.type() == NetworkRequestError::Unauthorized || err.myError.type() == NetworkRequestError::AuthFailed ) {
                                                           return true; //we try again
                                                         } else {
                                                           //try a different mirror
                                                           //return true;
                                                         }
                                                        }
                                                       return false; //stop
                                                     }
                                                   );
                                        })
                                      | to_vector()
                                      | waitFor<zyppng::expected<std::shared_ptr<BlockRequest>>>()
                                      | [ req ]( auto &&allResults ) {
                                          for ( const auto &res : allResults ) {
                                            if ( !res )
                                              return zyppng::expected<DlRequest>::error( res.error() );
                                          }
                                          return zyppng::expected<DlRequest>::success ( req );
                                        }
                                    ;

                                    return blockRequests;
                                  })
                     ;
                     return res;
                   }
                   return zyppng::makeReadyResult( zyppng::expected<DlRequest>::success( std::move(req) ) );
                 }

                 | mbind( []( auto &&req ){
                     return zyppng::mtry([&](){
                       return checksumFromDLRequest(req);
                   });
                 })
          ;
      })
    | to_vector()
    | waitFor<zyppng::expected<DlRequest>>()
    ;

  res->onReady( [ ctx ]( auto && ){
    ctx->ev->quit();
  });

  ctx->ev->run();
  return 0;
}
