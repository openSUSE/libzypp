/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaMultiCurl.cc
 *
*/

#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <glib.h>

#include <vector>
#include <iostream>
#include <algorithm>


#include <zypp/ZConfig.h>
#include <zypp/base/Logger.h>
#include <zypp/media/MediaMultiCurl.h>
#include <zypp-core/zyppng/base/private/linuxhelpers_p.h>
#include <zypp-curl/parser/MetaLinkParser>
#include <zypp-curl/parser/zsyncparser.h>
#include <zypp/ManagedFile.h>
#include <zypp-curl/private/curlhelper_p.h>
#include <zypp-curl/auth/CurlAuthData>
#include <zypp-curl/parser/metadatahelper.h>
#include <zypp-curl/ng/network/curlmultiparthandler.h>

using std::endl;
using namespace zypp::base;

#undef CURLVERSION_AT_LEAST
#define CURLVERSION_AT_LEAST(M,N,O) LIBCURL_VERSION_NUM >= ((((M)<<8)+(N))<<8)+(O)

namespace zypp {
  namespace media {

    using MultiByteHandler = zyppng::CurlMultiPartHandler;


/*!
 * General Idea how the MultiFetchRequest pulls data from the server
 *
 * We download files in ranges using a worker per mirror URL and we want to limit the number of requests we need to start
 * for a download. Since we can not control the size of the ranges we need from files, those
 * are defined by the delta algorithm, we need to fetch multiple ranges per request. This however
 * adds another factor, the delta argorithm marks all ranges that we can reuse already, but those are scattered
 * all across the file we need to download, meaning we will have a list of ranges with gaps between them.
 *
 * For example we need the ranges:  [(0,100), (200,100), (300,100), (1000, 100) ].
 * Because of the gaps we can not just coalesce many ranges into a big range (0, 1100), we need to utilize multi range HTTP requests
 * and fetch [(0,100), (200, 200), (1000, 100)] as multibyte HTTP requests.
 *
 * Based on the filesize we calculate the nr of bytes we want to fetch per request and group the required ranges
 * into what we call "Stripes". Doing that will help us with the stealing algorithm that is described later.
 *
 * The logic is split in 2 main parts:
 *  - multifetchrequest, which is the class that holds information about the full request itself
 *  - multifetchworker, which is the worker for a specific mirror working on a specific Stripe
 *
 * When the algorithm is started, multifetchrequest will spawn a worker ( up to configured max workers ) for each mirror URL,
 * then calls multifetchworker::nextjob() for each worker and waits for their results on a internal event loop.
 *
 * A worker when starting up will first go into a DNS check to see if the mirror it has does resolve, then it will look into the
 * request instance to see what the next stripe is it needs to download. There is a simple stripe index , called _stripeNo, in the
 * parent request that tells the worker what the next stripe is that needs fetching. If that number is still a valid index the worker
 * will increase it and claim the stripe for itself. Building a HTTP request and waiting for its result.
 *
 * Some servers do only support fetching a max number of ranges, in that instance the worker will finish the request and return true
 * for "hasMoreWork". In that cases the event loop in multifetchrequest will tell the worker to continue the job until all blocks were
 * fetched.
 *
 * Before writing to a block or starting a new block, a worker will first check if it was already marked as FINALIZED in the stripe, if that is the case the worker will
 * set itself to WORKER_DISCARD and ask for a new job, otherwise it will start writing into the block and set it to COMPETING.
 *
 * At the end of writing a block, the worker will first check if the data it received was valid via the given digest and go into WORKER_BROKEN if is not.
 * When valid data was received it checks the state of the block in the stripe, if it is set to FETCH the block is marked as
 * FINALIZED and considered to be done and valid. If the block is already marked as COMPETING, the worker will recalc the checksum and mark the block
 * as FINALIZED if the result is valid, otherwise the block is marked with REFETCH to force redownloading it.
 *
 * In case of a error, the request is stopped and the worker reports back to the multifetchrequest event loop which will set the
 * worker into a "Broken" state stopping it from fetching more jobs. If there are more mirrors available, it will spawn a new worker
 * which then can also start to claim jobs. This makes sure that even when we have errors, we try all mirrors until we run out of them.
 * The broken worker will not be deleted, instead it will stay in the list of workers until another one will "steal" the work from it and
 * hopefully finish it.
 *
 * Now, once all Stripes have been claimed and a worker was asked to get the next job it will start to steal from others. Meaning it will
 * go over the list of currently running workers and try to figure out the best candidate for stealing. This is based on the performance of the
 * worker and how many workers already compete on a stripe.
 *
 * - If the worker finds a best candidate it will start competition on the stripe and
 *    will request all ranges not marked as "FINALIZED" from the server as well. The worker finishing a block first wins, the one loosing will
 *    stop downloading and report back to get a next job if there is one.
 * - If the worker can not find a best candidate , it will set itself to "WORKER_DONE" and stop stealing stripes
 *
 *
 * Once a worker is done it needs to check if all blocks from its current stripe are marked as FINALIZED, otherwise they need to be refetched.
 */




class multifetchrequest;

struct Stripe {

  enum RState {
    PENDING,    //< Pending Range
    FETCH,      //< Fetch is running!
    COMPETING,  //< Competing workers, needs checksum recheck
    FINALIZED,  //< Done, don't write to it anymore
    REFETCH     //< This block needs a refetch
  };

  std::vector<off_t>  blocks;       //< required block numbers from blocklist
  std::vector<RState> blockStates;  //< current state of each block in blocks
};

enum MultiFetchWorkerState {
  WORKER_STARTING,
  WORKER_LOOKUP,
  WORKER_FETCH,
  WORKER_DISCARD,
  WORKER_DONE,
  WORKER_SLEEP,
  WORKER_BROKEN
};

// Hack: we derive from MediaCurl just to get the storage space for
// settings, url, curlerrors and the like
class multifetchworker : private MediaCurl, public zyppng::CurlMultiPartDataReceiver {
  friend class multifetchrequest;

public:
  multifetchworker(int no, multifetchrequest &request, const Url &url);
  ~multifetchworker();

  /*!
   * Fetches the next job from the parent request,
   * then calls runjob to execute it
   */
  void nextjob();

  /*!
   * Fetches all jobs from the currently claimed stripe or calls nextjob()
   * if the current stripe has all blocks marked as FINALIZED
   */
  void runjob();

  /*!
   * Continues a running job, only called by the dispatcher
   * if the multibyte handler indicates it has more work
   * to trigger fetching the next range batch
   */
  bool continueJob();

  /*!
   * Rechecks the checksum of the given block at \a blockIdx.
   * The index is relative to the workers blocklist
   */
  bool recheckChecksum( off_t blockIdx );

  /*!
   * Stop all competing workers and set them to WORKER_DISCARD
   */
  void disableCompetition();

  /*!
   * Starts the dns check
   */
  void checkdns();
  void adddnsfd( std::vector<GPollFD> &waitFds );
  void dnsevent(const std::vector<GPollFD> &waitFds );

  const int _workerno;

  MultiFetchWorkerState _state = WORKER_STARTING;
  bool _competing = false;

  std::vector<MultiByteHandler::Range> _blocks;
  std::vector<off_t>                   _rangeToStripeBlock;

  MultiByteHandler::ProtocolMode       _protocolMode = MultiByteHandler::ProtocolMode::Basic;
  std::unique_ptr<MultiByteHandler>    _multiByteHandler;

  off_t  _stripe   = 0; //< The current stripe we are downloading
  size_t _datasize = 0; //< The nr of bytes we need to download overall

  double _starttime    = 0; //< When was the current job started
  size_t _datareceived = 0; //< Data downloaded in the current job only
  off_t  _received     = 0; //< Overall data"MultiByteHandler::prepare failed" fetched by this worker

  double _avgspeed = 0;
  double _maxspeed = 0;

  double _sleepuntil = 0;

private:
  void run();
  void stealjob();
  bool setupHandle();
  MultiByteHandler::Range rangeFromBlock( off_t blockNo ) const;

  size_t writefunction  ( char *ptr, std::optional<off_t> offset, size_t bytes ) override;
  size_t headerfunction ( char *ptr, size_t bytes ) override;
  bool   beginRange     ( off_t range, std::string &cancelReason ) override;
  bool   finishedRange  ( off_t range, bool validated, std::string &cancelReason ) override;

  multifetchrequest *_request = nullptr;
  int _pass = 0;
  std::string _urlbuf;

  pid_t _pid   = 0;
  int _dnspipe = -1;
};

class multifetchrequest {
public:
  multifetchrequest(const MediaMultiCurl *context, const Pathname &filename, const Url &baseurl, CURLM *multi, FILE *fp, callback::SendReport<DownloadProgressReport> *report, MediaBlockList &&blklist, off_t filesize);
  ~multifetchrequest();

  void run(std::vector<Url> &urllist);
  static ByteCount makeBlksize( uint maxConns, size_t filesize );

  MediaBlockList &blockList() {
    return _blklist;
  }

protected:
  friend class multifetchworker;

  const MediaMultiCurl *_context;
  const Pathname _filename;
  Url _baseurl;

  FILE *_fp = nullptr;
  callback::SendReport<DownloadProgressReport> *_report = nullptr;
  MediaBlockList _blklist;

  std::vector<Stripe> _requiredStripes; // all the data we need

  off_t _filesize = 0; //< size of the file we want to download

  CURLM *_multi = nullptr;

  std::list< std::unique_ptr<multifetchworker> > _workers;
  bool _stealing = false;
  bool _havenewjob = false;

  zypp::ByteCount _defaultBlksize = 0; //< The blocksize to use if the metalink file does not specify one
  off_t  _stripeNo       = 0; //< next stripe to download

  size_t _activeworkers  = 0;
  size_t _lookupworkers  = 0;
  size_t _sleepworkers   = 0;
  double _minsleepuntil  = 0;
  bool _finished         = false;

  off_t _totalsize = 0; //< nr of bytes we need to download ( e.g. filesize - ( bytes reused from deltafile ) )
  off_t _fetchedsize = 0;
  off_t _fetchedgoodsize = 0;

  double _starttime    = 0;
  double _lastprogress = 0;

  double _lastperiodstart   = 0;
  double _lastperiodfetched = 0;
  double _periodavg         = 0;

public:
  double _timeout           = 0;
  double _connect_timeout   = 0;
  double _maxspeed          = 0;
  int _maxworkers           = 0;
};

constexpr auto MIN_REQ_MIRRS = 4;
constexpr auto MAXURLS       = 10;

//////////////////////////////////////////////////////////////////////

static double
currentTime()
{
#if _POSIX_C_SOURCE >= 199309L
  struct timespec ts;
  if ( clock_gettime( CLOCK_MONOTONIC, &ts) )
    return 0;
  return ts.tv_sec + ts.tv_nsec / 1000000000.;
#else
  struct timeval tv;
  if (gettimeofday(&tv, NULL))
    return 0;
  return tv.tv_sec + tv.tv_usec / 1000000.;
#endif
}

size_t
multifetchworker::writefunction(char *ptr, std::optional<off_t> offset, size_t bytes)
{
  if ( _state == WORKER_BROKEN || _state == WORKER_DISCARD )
    return bytes ? 0 : 1;

  double now = currentTime();

  // update stats of overall data
  _datareceived += bytes;
  _received += bytes;
  _request->_lastprogress = now;

  const auto &currRange = _multiByteHandler->currentRange();
  if (!currRange)
    return 0;  // we always write to a range

  auto &stripeDesc = _request->_requiredStripes[_stripe];
  if ( !_request->_fp || stripeDesc.blockStates[ _rangeToStripeBlock[*currRange] ] == Stripe::FINALIZED ) {
    // someone else finished our block first!
    // we stop here and fetch new jobs if there are still some
    _state     = WORKER_DISCARD;
    _competing = false;
    return 0;
  }

  const auto &blk = _blocks[*currRange];
  off_t seekTo = blk.start + blk.bytesWritten;

  if ( ftell( _request->_fp ) != seekTo ) {
    // if we can't seek the file there is no purpose in trying again
    if (fseeko(_request->_fp, seekTo, SEEK_SET))
      return bytes ? 0 : 1;
  }

  size_t cnt = fwrite(ptr, 1, bytes, _request->_fp);
  _request->_fetchedsize += cnt;
  return cnt;
}

bool multifetchworker::beginRange ( off_t workerRangeOff, std::string &cancelReason )
{
  auto &stripeDesc = _request->_requiredStripes[_stripe];
  auto stripeRangeOff = _rangeToStripeBlock[workerRangeOff];
  const auto &currRangeState = stripeDesc.blockStates[stripeRangeOff];

  if ( currRangeState == Stripe::FINALIZED ){
    cancelReason = "Cancelled because stripe block is already finalized";
    _state = WORKER_DISCARD;
    WAR << "#" << _workerno << ": trying to start a range ("<<stripeRangeOff<<"["<< _blocks[workerRangeOff].start <<" : "<<_blocks[workerRangeOff].len<<"]) that was already finalized, cancelling. Stealing was: " << _request->_stealing << endl;
    return false;
  }
  stripeDesc.blockStates[stripeRangeOff] = currRangeState == Stripe::PENDING ?  Stripe::FETCH : Stripe::COMPETING;
  return true;
}

bool multifetchworker::finishedRange ( off_t workerRangeOff, bool validated, std::string &cancelReason )
{
  auto &stripeDesc = _request->_requiredStripes[_stripe];
  auto stripeRangeOff = _rangeToStripeBlock[workerRangeOff];
  const auto &currRangeState = stripeDesc.blockStates[stripeRangeOff];

  if ( !validated ) {
    // fail, worker will go into WORKER_BROKEN
    cancelReason = "Block failed to validate";
    return false;
  }

  if ( currRangeState == Stripe::FETCH ) {
    // only us who wrote here, block is finalized
    stripeDesc.blockStates[stripeRangeOff] = Stripe::FINALIZED;
    _request->_fetchedgoodsize += _blocks[workerRangeOff].len;
  } else {
    // others wrote here, we need to check the full checksum
    if ( recheckChecksum ( workerRangeOff ) ) {
      stripeDesc.blockStates[stripeRangeOff] = Stripe::FINALIZED;
      _request->_fetchedgoodsize += _blocks[workerRangeOff].len;
    } else {
      // someone messed that block up, set it to refetch but continue since our
      // data is valid
      WAR << "#" << _workerno << ": Broken data in COMPETING block, requesting refetch. Stealing is:  " << _request->_stealing << endl;
      stripeDesc.blockStates[stripeRangeOff] = Stripe::REFETCH;
    }
  }
  return true;
}

size_t
multifetchworker::headerfunction( char *p, size_t bytes )
{
  size_t l = bytes;
  if (l > 9 && !strncasecmp(p, "Location:", 9)) {
    std::string line(p + 9, l - 9);
    if (line[l - 10] == '\r')
      line.erase(l - 10, 1);
    XXX << "#" << _workerno << ": redirecting to" << line << endl;
    return bytes;
  }

  const auto &repSize = _multiByteHandler->reportedFileSize ();
  if ( repSize && *repSize != _request->_filesize  ) {
      XXX << "#" << _workerno << ": filesize mismatch" << endl;
      _state = WORKER_BROKEN;
      strncpy(_curlError, "filesize mismatch", CURL_ERROR_SIZE);
      return 0;
  }

  return bytes;
}

multifetchworker::multifetchworker(int no, multifetchrequest &request, const Url &url)
: MediaCurl(url, Pathname())
, _workerno( no )
, _maxspeed( request._maxspeed )
, _request ( &request )
{
  Url curlUrl( clearQueryString(url) );
  _urlbuf = curlUrl.asString();
  _curl = _request->_context->fromEasyPool(_url.getHost());
  if (_curl)
    XXX << "reused worker from pool" << endl;
  if (!_curl && !(_curl = curl_easy_init()))
    {
      _state = WORKER_BROKEN;
      strncpy(_curlError, "curl_easy_init failed", CURL_ERROR_SIZE);
      return;
    }

  if ( url.getScheme() == "http" ||  url.getScheme() == "https" )
    _protocolMode = MultiByteHandler::ProtocolMode::HTTP;

  setupHandle();
  checkdns();
}

bool multifetchworker::setupHandle()
{
  try {
      setupEasy();
  } catch (Exception &ex) {
    curl_easy_cleanup(_curl);
    _curl = 0;
    _state = WORKER_BROKEN;
    strncpy(_curlError, "curl_easy_setopt failed", CURL_ERROR_SIZE);
    return false;
  }
  curl_easy_setopt(_curl, CURLOPT_PRIVATE, this);
  curl_easy_setopt(_curl, CURLOPT_URL, _urlbuf.c_str());

  // if this is the same host copy authorization
  // (the host check is also what curl does when doing a redirect)
  // (note also that unauthorized exceptions are thrown with the request host)
  if ( _url.getHost() == _request->_context->_url.getHost()) {
    _settings.setUsername(_request->_context->_settings.username());
    _settings.setPassword(_request->_context->_settings.password());
    _settings.setAuthType(_request->_context->_settings.authType());
    if ( _settings.userPassword().size() ) {
      curl_easy_setopt(_curl, CURLOPT_USERPWD, _settings.userPassword().c_str());
      std::string use_auth = _settings.authType();
      if (use_auth.empty())
        use_auth = "digest,basic";        // our default
      long auth = CurlAuthData::auth_type_str2long(use_auth);
      if( auth != CURLAUTH_NONE)
      {
        XXX << "#" << _workerno << ": Enabling HTTP authentication methods: " << use_auth
            << " (CURLOPT_HTTPAUTH=" << auth << ")" << std::endl;
        curl_easy_setopt(_curl, CURLOPT_HTTPAUTH, auth);
      }
    }
  }
  return true;
}

multifetchworker::~multifetchworker()
{
  if (_curl)
    {
      if (_state == WORKER_FETCH || _state == WORKER_DISCARD)
        curl_multi_remove_handle(_request->_multi, _curl);
      if (_state == WORKER_DONE || _state == WORKER_SLEEP)
        {
#if CURLVERSION_AT_LEAST(7,15,5)
          curl_easy_setopt(_curl, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t)0);
#endif
          curl_easy_setopt(_curl, CURLOPT_PRIVATE, (void *)0);
          curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, (void *)0);
          curl_easy_setopt(_curl, CURLOPT_WRITEDATA, (void *)0);
          curl_easy_setopt(_curl, CURLOPT_HEADERFUNCTION, (void *)0);
          curl_easy_setopt(_curl, CURLOPT_HEADERDATA, (void *)0);
          _request->_context->toEasyPool(_url.getHost(), _curl);
        }
      else
        curl_easy_cleanup(_curl);
      _curl = 0;
    }
  if (_pid)
    {
      kill(_pid, SIGKILL);
      int status;
      while (waitpid(_pid, &status, 0) == -1)
        if (errno != EINTR)
          break;
      _pid = 0;
    }
  if (_dnspipe != -1)
    {
      close(_dnspipe);
      _dnspipe = -1;
    }
  // the destructor in MediaCurl doesn't call disconnect() if
  // the media is not attached, so we do it here manually
  disconnectFrom();
}

static inline bool env_isset(std::string name)
{
  const char *s = getenv(name.c_str());
  return s && *s ? true : false;
}

void
multifetchworker::checkdns()
{
  std::string host = _url.getHost();

  if (host.empty())
    return;

  if (_request->_context->isDNSok(host))
    return;

  // no need to do dns checking for numeric hosts
  char addrbuf[128];
  if (inet_pton(AF_INET, host.c_str(), addrbuf) == 1)
    return;
  if (inet_pton(AF_INET6, host.c_str(), addrbuf) == 1)
    return;

  // no need to do dns checking if we use a proxy
  if (!_settings.proxy().empty())
    return;
  if (env_isset("all_proxy") || env_isset("ALL_PROXY"))
    return;
  std::string schemeproxy = _url.getScheme() + "_proxy";
  if (env_isset(schemeproxy))
    return;
  if (schemeproxy != "http_proxy")
    {
      std::transform(schemeproxy.begin(), schemeproxy.end(), schemeproxy.begin(), ::toupper);
      if (env_isset(schemeproxy))
        return;
    }

  XXX << "checking DNS lookup of " << host << endl;
  int pipefds[2];
  if (pipe(pipefds))
    {
      _state = WORKER_BROKEN;
      strncpy(_curlError, "DNS pipe creation failed", CURL_ERROR_SIZE);
      return;
    }
  _pid = fork();
  if (_pid == pid_t(-1))
    {
      close(pipefds[0]);
      close(pipefds[1]);
      _pid = 0;
      _state = WORKER_BROKEN;
      strncpy(_curlError, "DNS checker fork failed", CURL_ERROR_SIZE);
      return;
    }
  else if (_pid == 0)
    {
      close(pipefds[0]);
      // XXX: close all other file descriptors
      struct addrinfo *ai, aihints;
      memset(&aihints, 0, sizeof(aihints));
      aihints.ai_family = PF_UNSPEC;
      int tstsock = socket(PF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, 0);
      if (tstsock == -1)
        aihints.ai_family = PF_INET;
      else
        close(tstsock);
      aihints.ai_socktype = SOCK_STREAM;
      aihints.ai_flags = AI_CANONNAME;
      unsigned int connecttimeout = _request->_connect_timeout;
      if (connecttimeout)
        alarm(connecttimeout);
      signal(SIGALRM, SIG_DFL);
      if (getaddrinfo(host.c_str(), NULL, &aihints, &ai))
        _exit(1);
      _exit(0);
    }
  close(pipefds[1]);
  _dnspipe = pipefds[0];
  _state = WORKER_LOOKUP;
}

void
multifetchworker::adddnsfd(std::vector<GPollFD> &waitFds)
{
  if (_state != WORKER_LOOKUP)
    return;

  waitFds.push_back (
        GPollFD {
          .fd = _dnspipe,
          .events = G_IO_IN | G_IO_HUP | G_IO_ERR,
          .revents = 0
        });
}

void
multifetchworker::dnsevent( const std::vector<GPollFD> &waitFds )
{
  bool hasEvent = std::any_of( waitFds.begin (), waitFds.end(),[this]( const GPollFD &waitfd ){
    return ( waitfd.fd == _dnspipe && waitfd.revents != 0 );
  });

  if (_state != WORKER_LOOKUP || !hasEvent)
    return;
  int status;
  while (waitpid(_pid, &status, 0) == -1)
    {
      if (errno != EINTR)
        return;
    }
  _pid = 0;
  if (_dnspipe != -1)
    {
      close(_dnspipe);
      _dnspipe = -1;
    }
  if (!WIFEXITED(status))
    {
      _state = WORKER_BROKEN;
      strncpy(_curlError, "DNS lookup failed", CURL_ERROR_SIZE);
      _request->_activeworkers--;
      return;
    }
  int exitcode = WEXITSTATUS(status);
  XXX << "#" << _workerno << ": DNS lookup returned " << exitcode << endl;
  if (exitcode != 0)
    {
      _state = WORKER_BROKEN;
      strncpy(_curlError, "DNS lookup failed", CURL_ERROR_SIZE);
      _request->_activeworkers--;
      return;
    }
  _request->_context->setDNSok(_url.getHost());
  nextjob();
}

bool multifetchworker::recheckChecksum( off_t workerRangeIdx )
{
  // XXX << "recheckChecksum block " << _blkno << endl;
  if (!_request->_fp || !_datasize || !_blocks.size() )
    return true;

  auto &blk = _blocks[workerRangeIdx];
  if ( !blk._digest )
    return true;

  const auto currOf = ftell( _request->_fp );
  if ( currOf == -1 )
    return false;

  if (fseeko(_request->_fp, blk.start, SEEK_SET))
    return false;

  zypp::Digest newDig = blk._digest->clone();

  char buf[4096];
  size_t l = blk.len;
  while (l) {
    size_t cnt = l > sizeof(buf) ? sizeof(buf) : l;
    if (fread(buf, cnt, 1, _request->_fp) != 1)
      return false;
    newDig.update(buf, cnt);
    l -= cnt;
  }

  if (fseeko(_request->_fp, currOf, SEEK_SET))
    return false;

  blk._digest = std::move(newDig);
  if (!_multiByteHandler->validateRange(blk)) {
    WAR << "#" << _workerno << " Stripe: " << _stripe << ": Stripe-Block: " << _rangeToStripeBlock[workerRangeIdx] << " failed to validate" << endl;
    return false;
  }

  return true;
}

/*!
 * Calculates the range information for the given \a blkNo ( actual block index in the request blocklist, NOT the stripe or worker index )
 */
zyppng::CurlMultiPartHandler::Range multifetchworker::rangeFromBlock( off_t blkNo ) const
{
  UByteArray sum;
  std::optional<zypp::Digest> digest;
  std::optional<size_t> relDigLen;
  std::optional<size_t> blkSumPad;

  const auto &blk   = _request->_blklist.getBlock( blkNo );
  if ( _request->_blklist.haveChecksum ( blkNo ) ) {
    sum = _request->_blklist.getChecksum( blkNo );
    relDigLen = sum.size( );
    blkSumPad = _request->_blklist.checksumPad( );
    digest = zypp::Digest();
    digest->create( _request->_blklist.getChecksumType() );
  }

  return MultiByteHandler::Range::make(
        blk.off,
        blk.size,
        std::move(digest),
        std::move(sum),
        {}, // empty user data
        std::move(relDigLen),
        std::move(blkSumPad) );
}

void multifetchworker::stealjob()
{
  if (!_request->_stealing)
    {
      XXX << "start stealing!" << endl;
      _request->_stealing = true;
    }

  multifetchworker *best = 0; // best choice for the worker we want to compete with
  double now = 0;

  // look through all currently running workers to find the best candidate we
  // could steal from
  for (auto workeriter = _request->_workers.begin(); workeriter != _request->_workers.end(); ++workeriter)
    {
      multifetchworker *worker = workeriter->get();
      if (worker == this)
        continue;
      if (worker->_pass == -1)
        continue;	// do not steal!
      if (worker->_state == WORKER_DISCARD || worker->_state == WORKER_DONE || worker->_state == WORKER_SLEEP || !worker->_datasize)
        continue;	// do not steal finished jobs
      if (!worker->_avgspeed && worker->_datareceived)
        {
          // calculate avg speed for the worker if that was not done yet
          if (!now)
            now = currentTime();
          if (now > worker->_starttime)
            worker->_avgspeed = worker->_datareceived / (now - worker->_starttime);
        }
      // only consider worker who still have work
      if ( worker->_datasize - worker->_datareceived <= 0 )
        continue;
      if (!best || best->_pass > worker->_pass)
        {
          best = worker;
          continue;
        }
      if (best->_pass < worker->_pass)
        continue;
      // if it is the same stripe,  our current best choice is competing with the worker we are looking at
      // we need to check if we are faster than the fastest one competing for this stripe, so we want the best.
      // Otherwise the worst.
      if (worker->_stripe == best->_stripe)
        {
        if ((worker->_datasize - worker->_datareceived) * best->_avgspeed < (best->_datasize - best->_datareceived) * worker->_avgspeed)
            best = worker;
        }
      else
        {
        if ((worker->_datasize - worker->_datareceived) * best->_avgspeed > (best->_datasize - best->_datareceived) * worker->_avgspeed)
            best = worker;
        }
    }
  if (!best)
    {
      _state = WORKER_DONE;
      _request->_activeworkers--;
      _request->_finished = true;
      return;
    }
  // do not sleep twice
  if (_state != WORKER_SLEEP)
    {
    if (!_avgspeed && _datareceived)
        {
          if (!now)
            now = currentTime();
          if (now > _starttime)
            _avgspeed = _datareceived / (now - _starttime);
        }

      // lets see if we should sleep a bit
      XXX << "me #" << _workerno << ": " << _avgspeed << ", size " << best->_datasize << endl;
      XXX << "best #" << best->_workerno << ": " << best->_avgspeed << ", size " << (best->_datasize - best->_datareceived) << endl;

      // check if we could download the full data from best faster than best could download its remaining data
      if ( _avgspeed && best->_avgspeed                     // we and best have average speed information
          && _avgspeed <=  best->_avgspeed ) // and we are not faster than best
        {
          if (!now)
            now = currentTime();
          double sl = (best->_datasize - best->_datareceived) / best->_avgspeed * 2;
          if (sl > 1)
            sl = 1;
          XXX << "#" << _workerno << ": going to sleep for " << sl * 1000 << " ms" << endl;
          _sleepuntil = now + sl;
          _state = WORKER_SLEEP;
          _request->_sleepworkers++;
          return;
        }
    }

  _competing = true;
  best->_competing = true;
  _stripe = best->_stripe;

  best->_pass++;
  _pass = best->_pass;

  runjob();
}

void
multifetchworker::disableCompetition()
{
  for ( auto workeriter = _request->_workers.begin(); workeriter != _request->_workers.end(); ++workeriter)
    {
      multifetchworker *worker = workeriter->get();
      if (worker == this)
        continue;
      if (worker->_stripe == _stripe)
        {
          if (worker->_state == WORKER_FETCH)
            worker->_state = WORKER_DISCARD;
          worker->_pass = -1;	/* do not steal this one, we already have it */
        }
    }
}

void multifetchworker::nextjob()
{
  _datasize  = 0;
  _blocks.clear();

  // claim next stripe for us, or steal if there nothing left to claim
  if ( _request->_stealing || _request->_stripeNo >= _request->_requiredStripes.size() ) {
    stealjob();
    return;
  }

  _stripe = _request->_stripeNo++;
   runjob();
}

void multifetchworker::runjob()
{
  _datasize = 0;
  _blocks.clear ();
  _rangeToStripeBlock.clear ();

  auto &stripeDesc = _request->_requiredStripes[_stripe];
  for ( uint i = 0; i < stripeDesc.blocks.size(); i++ ) {
    // ignore verified and finalized ranges
    if( stripeDesc.blockStates[i] == Stripe::FINALIZED ) {
      continue;
    } else {
      _blocks.push_back( rangeFromBlock(stripeDesc.blocks[i]) );
      _rangeToStripeBlock.push_back( i );
      _datasize += _blocks.back().len;
    }
  }

  if ( _datasize == 0 ) {
    // no blocks left in this stripe
    _state = WORKER_DONE;
    _request->_activeworkers--;
    if ( !_request->_activeworkers )
      _request->_finished = true;
    return;
  }

  DBG << "#" << _workerno << "Done adding blocks to download, going to download: " << _blocks.size() << " nr of block with " << _datasize << " nr of bytes" << std::endl;

  _multiByteHandler.reset( nullptr );
  _multiByteHandler = std::make_unique<MultiByteHandler>(_protocolMode, _curl, _blocks, *this );
  _starttime        = currentTime();
  _datareceived     = 0;
  run();
}

bool multifetchworker::continueJob()
{
  bool hadRangeFail = _multiByteHandler->lastError() == MultiByteHandler::Code::RangeFail;
  if ( !_multiByteHandler->prepareToContinue() ) {
    strncpy( _curlError, _multiByteHandler->lastErrorMessage().c_str(), CURL_ERROR_SIZE );
    return false;
  }

  if ( hadRangeFail ) {
    // we reset the handle to default values. We do this to not run into
    // "transfer closed with outstanding read data remaining" error CURL sometimes returns when
    // we cancel a connection because of a range error to request a smaller batch.
    // The error will still happen but much less frequently than without resetting the handle.
    //
    // Note: Even creating a new handle will NOT fix the issue
    curl_easy_reset( _curl );
    if ( !setupHandle())
      return false;
  }

  run();
  return true;
}

void
multifetchworker::run()
{
  if (_state == WORKER_BROKEN || _state == WORKER_DONE)
    return;	// just in case...

  if ( !_multiByteHandler->prepare() ) {
    _request->_activeworkers--;
    _state = WORKER_BROKEN;
    strncpy( _curlError, _multiByteHandler->lastErrorMessage ().c_str() , CURL_ERROR_SIZE );
    return;
  }

  if (curl_multi_add_handle(_request->_multi, _curl) != CURLM_OK) {
    _request->_activeworkers--;
    _state = WORKER_BROKEN;
    strncpy( _curlError, "curl_multi_add_handle failed", CURL_ERROR_SIZE );
    return;
  }

  _request->_havenewjob = true;
  _state = WORKER_FETCH;
}


//////////////////////////////////////////////////////////////////////


multifetchrequest::multifetchrequest(const MediaMultiCurl *context, const Pathname &filename, const Url &baseurl, CURLM *multi, FILE *fp, callback::SendReport<DownloadProgressReport> *report, MediaBlockList &&blklist, off_t filesize)
  : _context(context)
  , _filename(filename)
  , _baseurl(baseurl)
  , _fp(fp)
  , _report(report)
  , _blklist(std::move(blklist))
  , _filesize(filesize)
  , _multi(multi)
  , _starttime(currentTime())
  , _timeout(context->_settings.timeout())
  , _connect_timeout(context->_settings.connectTimeout())
  , _maxspeed(context->_settings.maxDownloadSpeed())
  , _maxworkers(context->_settings.maxConcurrentConnections())
 {
  _lastperiodstart = _lastprogress = _starttime;
  if (_maxworkers > MAXURLS)
    _maxworkers = MAXURLS;
  if (_maxworkers <= 0)
    _maxworkers = 1;

  // calculate the total size of our download
  for (size_t blkno = 0; blkno < _blklist.numBlocks(); blkno++)
    _totalsize += _blklist.getBlock(blkno).size;

  // equally distribute the data we want to download over all workers
  _defaultBlksize = makeBlksize( _maxworkers, _totalsize );

  // lets build stripe informations
  zypp::ByteCount currStripeSize = 0;
  for (size_t blkno = 0; blkno < _blklist.numBlocks(); blkno++) {

    const MediaBlock &blk = _blklist.getBlock(blkno);
    if ( _requiredStripes.empty() || currStripeSize >= _defaultBlksize ) {
      _requiredStripes.push_back( Stripe{} );
      currStripeSize = 0;
    }

    _requiredStripes.back().blocks.push_back(blkno);
    _requiredStripes.back().blockStates.push_back(Stripe::PENDING);
    currStripeSize += blk.size;
  }

  MIL << "Downloading " << _blklist.numBlocks() << " blocks via " << _requiredStripes.size() << " stripes on " << _maxworkers << " connections." << endl;
}

multifetchrequest::~multifetchrequest()
{
  _workers.clear();
}

void
multifetchrequest::run(std::vector<Url> &urllist)
{
  int workerno = 0;
  std::vector<Url>::iterator urliter = urllist.begin();

  struct CurlMuliSockHelper {
    CurlMuliSockHelper( multifetchrequest &p ) : _parent(p) {
      curl_multi_setopt( _parent._multi, CURLMOPT_SOCKETFUNCTION, socketcb );
      curl_multi_setopt( _parent._multi, CURLMOPT_SOCKETDATA, this );
      curl_multi_setopt( _parent._multi, CURLMOPT_TIMERFUNCTION, timercb );
      curl_multi_setopt( _parent._multi, CURLMOPT_TIMERDATA, this );
    }

    ~CurlMuliSockHelper() {
      curl_multi_setopt( _parent._multi, CURLMOPT_SOCKETFUNCTION, nullptr );
      curl_multi_setopt( _parent._multi, CURLMOPT_SOCKETDATA, nullptr );
      curl_multi_setopt( _parent._multi, CURLMOPT_TIMERFUNCTION, nullptr );
      curl_multi_setopt( _parent._multi, CURLMOPT_TIMERDATA, nullptr );
    }

    static int socketcb (CURL * easy, curl_socket_t s, int what, CurlMuliSockHelper *userp, void *sockp ) {
      auto it = std::find_if( userp->socks.begin(), userp->socks.end(), [&]( const GPollFD &fd){ return fd.fd == s; });
      gushort events = 0;
      if ( what == CURL_POLL_REMOVE ) {
        if ( it == userp->socks.end() ) {
          WAR << "Ignoring unknown socket in static_socketcb" << std::endl;
          return 0;
        }
        userp->socks.erase(it);
        return 0;
      } else if ( what == CURL_POLL_IN ) {
        events = G_IO_IN | G_IO_HUP | G_IO_ERR;
      } else if ( what == CURL_POLL_OUT ) {
        events = G_IO_OUT | G_IO_ERR;
      } else if ( what == CURL_POLL_INOUT ) {
        events = G_IO_IN | G_IO_OUT | G_IO_HUP | G_IO_ERR;
      }

      if ( it != userp->socks.end() ) {
        it->events = events;
        it->revents = 0;
      } else {
        userp->socks.push_back(
          GPollFD{
            .fd = s,
            .events = events,
            .revents = 0
          }
        );
      }

      return 0;
    }

    //called by curl to setup a timer
    static int timercb( CURLM *, long timeout_ms, CurlMuliSockHelper *thatPtr ) {
      if ( !thatPtr )
        return 0;
      if ( timeout_ms == -1 )
        thatPtr->timeout_ms.reset(); // curl wants to delete its timer
      else
        thatPtr->timeout_ms = timeout_ms; // maximum time curl wants us to sleep
      return 0;
    }

    multifetchrequest &_parent;
    std::vector<GPollFD> socks;
    std::optional<long> timeout_ms = 0; //if set curl wants a timeout
  } _curlHelper(*this) ;

  // kickstart curl
  int handles = 0;
  CURLMcode mcode = curl_multi_socket_action( _multi, CURL_SOCKET_TIMEOUT, 0, &handles );
  if (mcode != CURLM_OK)
    ZYPP_THROW(MediaCurlException(_baseurl, "curl_multi_socket_action", "unknown error"));

  for (;;)
    {
      // list of all fds we want to poll
      std::vector<GPollFD> waitFds;
      int dnsFdCount = 0;

      if (_finished)
        {
          XXX << "finished!" << endl;
          break;
        }

      if ((int)_activeworkers < _maxworkers && urliter != urllist.end() && _workers.size() < MAXURLS)
        {
          // spawn another worker!
          _workers.push_back(std::make_unique<multifetchworker>(workerno++, *this, *urliter));
          auto &worker = _workers.back();
          if (worker->_state != WORKER_BROKEN)
            {
              _activeworkers++;
              if (worker->_state != WORKER_LOOKUP)
                {
                  worker->nextjob();
                }
              else
                _lookupworkers++;
            }
          ++urliter;
          continue;
        }
      if (!_activeworkers)
        {
          WAR << "No more active workers!" << endl;
          // show the first worker error we find
          for (auto workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
            {
              if ((*workeriter)->_state != WORKER_BROKEN)
                continue;
              ZYPP_THROW(MediaCurlException(_baseurl, "Server error", (*workeriter)->_curlError));
            }
          break;
        }

      if (_lookupworkers)
        for (auto workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
          (*workeriter)->adddnsfd( waitFds );

      // if we added a new job we have to call multi_perform once
      // to make it show up in the fd set. do not sleep in this case.
      int timeoutMs = _havenewjob ? 0 : 200;
      if ( _sleepworkers && !_havenewjob ) {
        if (_minsleepuntil == 0) {
          for (auto workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter) {
            multifetchworker *worker = workeriter->get();
            if (worker->_state != WORKER_SLEEP)
              continue;
            if (!_minsleepuntil || _minsleepuntil > worker->_sleepuntil)
              _minsleepuntil = worker->_sleepuntil;
          }
        }
        double sl = _minsleepuntil - currentTime();
        if (sl < 0) {
          sl = 0;
          _minsleepuntil = 0;
        }
        if (sl < .2)
          timeoutMs = sl * 1000;
      }

      if ( _curlHelper.timeout_ms.has_value() )
        timeoutMs = std::min<long>( timeoutMs, _curlHelper.timeout_ms.value() );

      dnsFdCount = waitFds.size(); // remember how many dns fd's we have
      waitFds.insert( waitFds.end(), _curlHelper.socks.begin(), _curlHelper.socks.end() ); // add the curl fd's to the poll data

      int r = zyppng::eintrSafeCall( g_poll, waitFds.data(), waitFds.size(), timeoutMs );
      if ( r == -1 )
        ZYPP_THROW(MediaCurlException(_baseurl, "g_poll() failed", "unknown error"));
      if ( r != 0 && _lookupworkers ) {
        for (auto workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
          {
            multifetchworker *worker = workeriter->get();
            if (worker->_state != WORKER_LOOKUP)
              continue;
            (*workeriter)->dnsevent( waitFds );
            if (worker->_state != WORKER_LOOKUP)
              _lookupworkers--;
          }
      }
      _havenewjob = false;

      // run curl
      if ( r == 0 ) {
        int handles = 0;
        CURLMcode mcode = curl_multi_socket_action( _multi, CURL_SOCKET_TIMEOUT, 0, &handles );
        if (mcode != CURLM_OK)
          ZYPP_THROW(MediaCurlException(_baseurl, "curl_multi_socket_action", "unknown error"));
      } else {
        for ( int sock = dnsFdCount; sock < waitFds.size(); sock++ ) {
          const auto &waitFd = waitFds[sock];
          if ( waitFd.revents == 0 )
            continue;

          int ev = 0;
          if ( (waitFd.revents & G_IO_HUP) == G_IO_HUP
              || (waitFd.revents & G_IO_IN) == G_IO_IN ) {
            ev = CURL_CSELECT_IN;
          }
          if ( (waitFd.revents & G_IO_OUT) == G_IO_OUT ) {
            ev |= CURL_CSELECT_OUT;
          }
          if ( (waitFd.revents & G_IO_ERR) == G_IO_ERR ) {
            ev |= CURL_CSELECT_ERR;
          }

          int runn = 0;
          CURLMcode mcode = curl_multi_socket_action( _multi, waitFd.fd, ev, &runn );
          if (mcode != CURLM_OK)
            ZYPP_THROW(MediaCurlException(_baseurl, "curl_multi_socket_action", "unknown error"));
        }
      }

      double now = currentTime();

      // update periodavg
      if (now > _lastperiodstart + .5)
        {
          if (!_periodavg)
            _periodavg = (_fetchedsize - _lastperiodfetched) / (now - _lastperiodstart);
          else
            _periodavg = (_periodavg + (_fetchedsize - _lastperiodfetched) / (now - _lastperiodstart)) / 2;
          _lastperiodfetched = _fetchedsize;
          _lastperiodstart = now;
        }

      // wake up sleepers
      if (_sleepworkers)
        {
          for (auto workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
            {
              multifetchworker *worker = workeriter->get();
              if (worker->_state != WORKER_SLEEP)
                continue;
              if (worker->_sleepuntil > now)
                continue;
              if (_minsleepuntil == worker->_sleepuntil)
                _minsleepuntil = 0;
              XXX << "#" << worker->_workerno << ": sleep done, wake up" << endl;
              _sleepworkers--;
              // nextjob changes the state
              worker->nextjob();
            }
        }

      // collect all curl results, (re)schedule jobs
      CURLMsg *msg;
      int nqueue;
      while ((msg = curl_multi_info_read(_multi, &nqueue)) != 0)
        {
          if (msg->msg != CURLMSG_DONE)
            continue;
          CURL *easy = msg->easy_handle;
          CURLcode cc = msg->data.result;
          multifetchworker *worker;

          if (curl_easy_getinfo(easy, CURLINFO_PRIVATE, &worker) != CURLE_OK)
            ZYPP_THROW(MediaCurlException(_baseurl, "curl_easy_getinfo", "unknown error"));

          if (worker->_datareceived && now > worker->_starttime) {
            if (worker->_avgspeed)
              worker->_avgspeed = (worker->_avgspeed + worker->_datareceived / (now - worker->_starttime)) / 2;
            else
              worker->_avgspeed = worker->_datareceived / (now - worker->_starttime);
          }

          XXX << "#" << worker->_workerno << " done code " << cc << " speed " << worker->_avgspeed << endl;
          curl_multi_remove_handle(_multi, easy);

          const auto &setWorkerBroken = [&]( const std::string &str = {} ){
            worker->_state = WORKER_BROKEN;
            if ( !str.empty () )
              strncpy(worker->_curlError, str.c_str(), CURL_ERROR_SIZE);
            _activeworkers--;

            if (!_activeworkers && !(urliter != urllist.end() && _workers.size() < MAXURLS)) {
              // end of workers reached! goodbye!
              worker->evaluateCurlCode(Pathname(), cc, false);
            }
          };

          if ( !worker->_multiByteHandler ) {
            WAR << "#" << worker->_workerno << ": has no multibyte handler, this is a bug" << endl;
            setWorkerBroken("Multibyte handler error");
            continue;
          }

          // tell the worker to finalize the current block
          worker->_multiByteHandler->finalize();

          if ( worker->_multiByteHandler->hasMoreWork() && ( cc == CURLE_OK || worker->_multiByteHandler->canRecover() ) ) {

            WAR << "#" << worker->_workerno << ": still has work to do or can recover from a error, continuing the job!" << endl;
            // the current job is not done, or we failed and need to try more, enqueue and start again
            if ( !worker->continueJob() ) {
              WAR << "#" << worker->_workerno << ": failed to continue (" << worker->_multiByteHandler->lastErrorMessage() << ")" << endl;
              setWorkerBroken( worker->_multiByteHandler->lastErrorMessage() );
            }
            continue;
          }

          // --- from here on worker has no more ranges in its current job, or had a error it can't recover from ---

          if ( cc != CURLE_OK ) {
            if ( worker->_state != WORKER_DISCARD ) {
              // something went wrong and we can not recover, broken worker!
              setWorkerBroken();
              continue;
            } else {
              WAR << "#" << worker->_workerno << ": failed, but was set to discard, reusing for new requests" << endl;
            }
          } else {

            // we got what we asked for, maybe. Lets see if all ranges have been marked as finalized
            if( !worker->_multiByteHandler->verifyData() ) {
              WAR << "#" << worker->_workerno << ": error: " << worker->_multiByteHandler->lastErrorMessage() << ", disable worker" << endl;
              setWorkerBroken();
              continue;
            }

            // from here on we know THIS worker only got data that verified
            // now lets see if the stripe was finished too
            // stripe blocks can now be only in FINALIZED or ERROR states
            if (worker->_state == WORKER_FETCH ) {
              if ( worker->_competing ) {
                worker->disableCompetition ();
              }
              auto &wrkerStripe = _requiredStripes[worker->_stripe];
              bool done = std::all_of( wrkerStripe.blockStates.begin(), wrkerStripe.blockStates.begin(), []( const Stripe::RState s ) { return s == Stripe::FINALIZED; } );
              if ( !done ) {
                // all ranges that are not finalized are in a bogus state, refetch them
                std::for_each( wrkerStripe.blockStates.begin(), wrkerStripe.blockStates.begin(), []( Stripe::RState &s ) {
                  if ( s != Stripe::FINALIZED)
                    s = Stripe::PENDING;
                });

                _finished = false; //reset finished flag
                worker->runjob();
                continue;
              }
            }

            // make bad workers ( bad as in really slow ) sleep a little
            double maxavg = 0;
            int maxworkerno = 0;
            int numbetter = 0;
            for (auto workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
              {
                multifetchworker *oworker = workeriter->get();
                if (oworker->_state == WORKER_BROKEN)
                  continue;
                if (oworker->_avgspeed > maxavg)
                  {
                    maxavg = oworker->_avgspeed;
                    maxworkerno = oworker->_workerno;
                  }
                if (oworker->_avgspeed > worker->_avgspeed)
                  numbetter++;
              }
            if (maxavg && !_stealing)
              {
                double ratio = worker->_avgspeed / maxavg;
                ratio = 1 - ratio;
                if (numbetter < 3)	// don't sleep that much if we're in the top two
                  ratio = ratio * ratio;
                if (ratio > .01)
                  {
                    XXX << "#" << worker->_workerno << ": too slow ("<< ratio << ", " << worker->_avgspeed << ", #" << maxworkerno << ": " << maxavg << "), going to sleep for " << ratio * 1000 << " ms" << endl;
                    worker->_sleepuntil = now + ratio;
                    worker->_state = WORKER_SLEEP;
                    _sleepworkers++;
                    continue;
                  }
              }

            // do rate control (if requested)
            // should use periodavg, but that's not what libcurl does
            if (_maxspeed && now > _starttime)
              {
                double avg = _fetchedsize / (now - _starttime);
                avg = worker->_maxspeed * _maxspeed / avg;
                if (avg < _maxspeed / _maxworkers)
                  avg = _maxspeed / _maxworkers;
                if (avg > _maxspeed)
                  avg = _maxspeed;
                if (avg < 1024)
                  avg = 1024;
                worker->_maxspeed = avg;
#if CURLVERSION_AT_LEAST(7,15,5)
                curl_easy_setopt(worker->_curl, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t)(avg));
#endif
              }

            worker->nextjob();
          }

          if ( _filesize > 0 && _fetchedgoodsize > _filesize ) {
            ZYPP_THROW(MediaFileSizeExceededException(_baseurl, _filesize));
          }
        }

      // send report
      if (_report)
        {
          int percent = _totalsize ? (100 * (_fetchedgoodsize + _fetchedsize)) / (_totalsize + _fetchedsize) : 0;

          double avg = 0;
          if (now > _starttime)
            avg = _fetchedsize / (now - _starttime);
          if (!(*(_report))->progress(percent, _baseurl, avg, _lastperiodstart == _starttime ? avg : _periodavg))
            ZYPP_THROW(MediaCurlException(_baseurl, "User abort", "cancelled"));
        }

      if (_timeout && now - _lastprogress > _timeout)
        break;
    }

  if (!_finished)
    ZYPP_THROW(MediaTimeoutException(_baseurl));

  // print some download stats
  WAR << "overall result" << endl;
  for (auto workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
    {
      multifetchworker *worker = workeriter->get();
      WAR << "#" << worker->_workerno << ": state: " << worker->_state << " received: " << worker->_received << " url: " << worker->_url << endl;
    }
}

inline zypp::ByteCount multifetchrequest::makeBlksize ( uint maxConns, size_t filesize )
{
  return std::max<zypp::ByteCount>( filesize / std::min( std::max<int>( 1, maxConns ) , MAXURLS ), zypp::ByteCount(4, zypp::ByteCount::K) );
}

//////////////////////////////////////////////////////////////////////


MediaMultiCurl::MediaMultiCurl(const Url &url_r, const Pathname & attach_point_hint_r)
    : MediaCurl(url_r, attach_point_hint_r)
{
  MIL << "MediaMultiCurl::MediaMultiCurl(" << url_r << ", " << attach_point_hint_r << ")" << endl;
  _multi = 0;
  _customHeadersMetalink = 0;
}

MediaMultiCurl::~MediaMultiCurl()
{
  if (_customHeadersMetalink)
    {
      curl_slist_free_all(_customHeadersMetalink);
      _customHeadersMetalink = 0;
    }
  if (_multi)
    {
      curl_multi_cleanup(_multi);
      _multi = 0;
    }
  std::map<std::string, CURL *>::iterator it;
  for (it = _easypool.begin(); it != _easypool.end(); it++)
    {
      CURL *easy = it->second;
      if (easy)
        {
          curl_easy_cleanup(easy);
          it->second = NULL;
        }
    }
}

void MediaMultiCurl::setupEasy()
{
  MediaCurl::setupEasy();

  if (_customHeadersMetalink)
    {
      curl_slist_free_all(_customHeadersMetalink);
      _customHeadersMetalink = 0;
    }
  struct curl_slist *sl = _customHeaders;
  for (; sl; sl = sl->next)
    _customHeadersMetalink = curl_slist_append(_customHeadersMetalink, sl->data);
  //, application/x-zsync
  _customHeadersMetalink = curl_slist_append(_customHeadersMetalink, "Accept: */*, application/x-zsync, application/metalink+xml, application/metalink4+xml");
}

// here we try to suppress all progress coming from a metalink download
// bsc#1021291: Nevertheless send alive trigger (without stats), so UIs
// are able to abort a hanging metalink download via callback response.
int MediaMultiCurl::progressCallback( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
  CURL *_curl = MediaCurl::progressCallback_getcurl(clientp);
  if (!_curl)
    return MediaCurl::aliveCallback(clientp, dltotal, dlnow, ultotal, ulnow);

  // bsc#408814: Don't report any sizes before we don't have data on disk. Data reported
  // due to redirection etc. are not interesting, but may disturb filesize checks.
  FILE *fp = 0;
  if ( curl_easy_getinfo( _curl, CURLINFO_PRIVATE, &fp ) != CURLE_OK || !fp )
    return MediaCurl::aliveCallback( clientp, dltotal, dlnow, ultotal, ulnow );
  if ( ftell( fp ) == 0 )
    return MediaCurl::aliveCallback( clientp, dltotal, 0.0, ultotal, ulnow );

  // (no longer needed due to the filesize check above?)
  // work around curl bug that gives us old data
  long httpReturnCode = 0;
  if (curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &httpReturnCode ) != CURLE_OK || httpReturnCode == 0)
    return MediaCurl::aliveCallback(clientp, dltotal, dlnow, ultotal, ulnow);

  char *ptr = NULL;
  bool ismetalink = false;
  if (curl_easy_getinfo(_curl, CURLINFO_CONTENT_TYPE, &ptr) == CURLE_OK && ptr)
    {
      std::string ct = std::string(ptr);
      if (ct.find("application/x-zsync") == 0 || ct.find("application/metalink+xml") == 0 || ct.find("application/metalink4+xml") == 0)
        ismetalink = true;
    }
  if (!ismetalink && dlnow < 256)
    {
      // can't tell yet, ...
      return MediaCurl::aliveCallback(clientp, dltotal, dlnow, ultotal, ulnow);
    }
  if (!ismetalink)
    {
      fflush(fp);
      ismetalink = looks_like_meta_file(fp) != MetaDataType::None;
      DBG << "looks_like_meta_file: " << ismetalink << endl;
    }
  if (ismetalink)
    {
      // this is a metalink file change the expected filesize
      MediaCurl::resetExpectedFileSize( clientp, ByteCount( 2, ByteCount::MB) );
      // we're downloading the metalink file. Just trigger aliveCallbacks
      curl_easy_setopt(_curl, CURLOPT_XFERINFOFUNCTION, &MediaCurl::aliveCallback);
      return MediaCurl::aliveCallback(clientp, dltotal, dlnow, ultotal, ulnow);
    }
  curl_easy_setopt(_curl, CURLOPT_XFERINFOFUNCTION, &MediaCurl::progressCallback);
  return MediaCurl::progressCallback(clientp, dltotal, dlnow, ultotal, ulnow);
}

void MediaMultiCurl::doGetFileCopy( const OnMediaLocation &srcFile , const Pathname & target, callback::SendReport<DownloadProgressReport> & report, RequestOptions options ) const
{
  Pathname dest = target.absolutename();
  if( assert_dir( dest.dirname() ) )
  {
    DBG << "assert_dir " << dest.dirname() << " failed" << endl;
    ZYPP_THROW( MediaSystemException(getFileUrl(srcFile.filename()), "System error on " + dest.dirname().asString()) );
  }

  ManagedFile destNew { target.extend( ".new.zypp.XXXXXX" ) };
  AutoFILE file;
  {
    AutoFREE<char> buf { ::strdup( (*destNew).c_str() ) };
    if( ! buf )
    {
      ERR << "out of memory for temp file name" << endl;
      ZYPP_THROW(MediaSystemException(getFileUrl(srcFile.filename()), "out of memory for temp file name"));
    }

    AutoFD tmp_fd { ::mkostemp( buf, O_CLOEXEC ) };
    if( tmp_fd == -1 )
    {
      ERR << "mkstemp failed for file '" << destNew << "'" << endl;
      ZYPP_THROW(MediaWriteException(destNew));
    }
    destNew = ManagedFile( (*buf), filesystem::unlink );

    file = ::fdopen( tmp_fd, "we" );
    if ( ! file )
    {
      ERR << "fopen failed for file '" << destNew << "'" << endl;
      ZYPP_THROW(MediaWriteException(destNew));
    }
    tmp_fd.resetDispose();	// don't close it here! ::fdopen moved ownership to file
  }

  DBG << "dest: " << dest << endl;
  DBG << "temp: " << destNew << endl;

  // set IFMODSINCE time condition (no download if not modified)
  if( PathInfo(target).isExist() && !(options & OPTION_NO_IFMODSINCE) )
  {
    curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
    curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, (long)PathInfo(target).mtime());
  }
  else
  {
    curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
    curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, 0L);
  }
  // change header to include Accept: metalink
  curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _customHeadersMetalink);
  // change to our own progress funcion
  curl_easy_setopt(_curl, CURLOPT_XFERINFOFUNCTION, &progressCallback);
  curl_easy_setopt(_curl, CURLOPT_PRIVATE, (*file) );	// important to pass the FILE* explicitly (passing through varargs)
  try
    {
      MediaCurl::doGetFileCopyFile( srcFile, dest, file, report, options );
    }
  catch (Exception &ex)
    {
      curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
      curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, 0L);
      curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _customHeaders);
      curl_easy_setopt(_curl, CURLOPT_PRIVATE, (void *)0);
      ZYPP_RETHROW(ex);
    }
  curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
  curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, 0L);
  curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _customHeaders);
  curl_easy_setopt(_curl, CURLOPT_PRIVATE, (void *)0);
  long httpReturnCode = 0;
  CURLcode infoRet = curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &httpReturnCode);
  if (infoRet == CURLE_OK)
  {
    DBG << "HTTP response: " + str::numstring(httpReturnCode) << endl;
    if ( httpReturnCode == 304
         || ( httpReturnCode == 213 && _url.getScheme() == "ftp" ) ) // not modified
    {
      DBG << "not modified: " << PathInfo(dest) << endl;
      return;
    }
  }
  else
  {
    WAR << "Could not get the response code." << endl;
  }

  MetaDataType ismetalink = MetaDataType::None;

  char *ptr = NULL;
  if (curl_easy_getinfo(_curl, CURLINFO_CONTENT_TYPE, &ptr) == CURLE_OK && ptr)
    {
      std::string ct = std::string(ptr);
      if (ct.find("application/x-zsync") == 0 )
        ismetalink = MetaDataType::Zsync;
      else if (ct.find("application/metalink+xml") == 0 || ct.find("application/metalink4+xml") == 0)
        ismetalink = MetaDataType::MetaLink;
    }

  if ( ismetalink == MetaDataType::None )
    {
      // some proxies do not store the content type, so also look at the file to find
      // out if we received a metalink (bnc#649925)
      fflush(file);
      ismetalink = looks_like_meta_file(destNew);
    }

  if ( ismetalink != MetaDataType::None )
    {
      bool userabort = false;
      Pathname failedFile = ZConfig::instance().repoCachePath() / "MultiCurl.failed";
      file = nullptr;	// explicitly close destNew before the parser reads it.
      try
        {
          MediaBlockList bl;
          std::vector<Url> urls;
          if ( ismetalink == MetaDataType::Zsync ) {
            ZsyncParser parser;
            parser.parse( destNew );
            bl = parser.getBlockList();
            urls = parser.getUrls();

            XXX << getFileUrl(srcFile.filename()) << " returned zsync meta data." << std::endl;
          } else {
            MetaLinkParser mlp;
            mlp.parse(destNew);
            bl = mlp.getBlockList();
            urls = mlp.getUrls();

            XXX << getFileUrl(srcFile.filename()) << " returned metalink meta data." << std::endl;
          }

          if ( bl.numBlocks() )
            XXX << "With " << bl.numBlocks() << " nr of blocks and a blocksize of " << bl.getBlock(0).size << std::endl;
          else
            XXX << "With no blocks" << std::endl;

          /*
           * gihub issue libzipp:#277 Multicurl backend breaks with MirrorCache and Metalink with unknown filesize.
           * Fall back to a normal download if we have no knowledge about the filesize we want to download.
           */
          if ( !bl.haveFilesize() && ! srcFile.downloadSize() ) {
            XXX << "No filesize in metalink file and no expected filesize, aborting multicurl." << std::endl;
            ZYPP_THROW( MediaException("Multicurl requires filesize but none was provided.") );
          }

#if 0
          Disabling this workaround for now, since we now do zip ranges into bigger requests
          /*
           * bsc#1191609 In certain locations we do not receive a suitable number of metalink mirrors, and might even
           * download chunks serially from one and the same server. In those cases we need to fall back to a normal download.
           */
          if ( urls.size() < MIN_REQ_MIRRS ) {
            ZYPP_THROW( MediaException("Multicurl enabled but not enough mirrors provided") );
          }
#endif

          // XXX << bl << endl;
          file = fopen((*destNew).c_str(), "w+e");
          if (!file)
            ZYPP_THROW(MediaWriteException(destNew));
          if (PathInfo(target).isExist())
            {
              XXX << "reusing blocks from file " << target << endl;
              bl.reuseBlocks(file, target.asString());
              XXX << bl << endl;
            }
          if (bl.haveChecksum(1) && PathInfo(failedFile).isExist())
            {
              XXX << "reusing blocks from file " << failedFile << endl;
              bl.reuseBlocks(file, failedFile.asString());
              XXX << bl << endl;
              filesystem::unlink(failedFile);
            }
          Pathname df = srcFile.deltafile();
          if (!df.empty())
            {
              XXX << "reusing blocks from file " << df << endl;
              bl.reuseBlocks(file, df.asString());
              XXX << bl << endl;
            }
          try
            {
              multifetch(srcFile.filename(), file, &urls, &report, std::move(bl), srcFile.downloadSize());
            }
          catch (MediaCurlException &ex)
            {
              userabort = ex.errstr() == "User abort";
              ZYPP_RETHROW(ex);
            }
        }
      catch (MediaFileSizeExceededException &ex) {
        ZYPP_RETHROW(ex);
      }
      catch (Exception &ex)
        {
          // something went wrong. fall back to normal download
          file = nullptr;	// explicitly close destNew before moving it
          WAR<< "Failed to multifetch file " << ex << " falling back to single Curl download!" << std::endl;
          if (PathInfo(destNew).size() >= 63336)
            {
              ::unlink(failedFile.asString().c_str());
              filesystem::hardlinkCopy(destNew, failedFile);
            }
          if (userabort)
            {
              ZYPP_RETHROW(ex);
            }
          file = fopen((*destNew).c_str(), "w+e");
          if (!file)
            ZYPP_THROW(MediaWriteException(destNew));

          // use the default progressCallback
          curl_easy_setopt(_curl, CURLOPT_XFERINFOFUNCTION, &MediaCurl::progressCallback);
          MediaCurl::doGetFileCopyFile(srcFile, dest, file, report, options | OPTION_NO_REPORT_START);
        }
    }

  if (::fchmod( ::fileno(file), filesystem::applyUmaskTo( 0644 )))
    {
      ERR << "Failed to chmod file " << destNew << endl;
    }

  file.resetDispose();	// we're going to close it manually here
  if (::fclose(file))
    {
      filesystem::unlink(destNew);
      ERR << "Fclose failed for file '" << destNew << "'" << endl;
      ZYPP_THROW(MediaWriteException(destNew));
    }

  if ( rename( destNew, dest ) != 0 )
    {
      ERR << "Rename failed" << endl;
      ZYPP_THROW(MediaWriteException(dest));
    }
  destNew.resetDispose();	// no more need to unlink it

  DBG << "done: " << PathInfo(dest) << endl;
}

void MediaMultiCurl::multifetch(const Pathname & filename, FILE *fp, std::vector<Url> *urllist, MediaBlockList &&blklist, callback::SendReport<DownloadProgressReport> *report, off_t filesize) const
{
  Url baseurl(getFileUrl(filename));
  if (filesize == off_t(-1) && blklist.haveFilesize())
    filesize = blklist.getFilesize();
  if (!blklist.haveBlocks() && filesize != 0) {
    if ( filesize == -1 ) {
      ZYPP_THROW(MediaException("No filesize and no blocklist, falling back to normal download."));
    }

    // build a blocklist on demand just so that we have ranges
    MIL << "Generate blocklist, since there was none in the metalink file." << std::endl;

    off_t currOff = 0;
    const auto prefSize = multifetchrequest::makeBlksize( _settings.maxConcurrentConnections(), filesize );

    while ( currOff <  filesize )  {

      auto blksize = filesize - currOff ;
      if ( blksize > prefSize )
        blksize = prefSize;

      blklist.addBlock( currOff, blksize );
      currOff += blksize;
    }

    XXX << "Generated blocklist: " << std::endl << blklist << std::endl << " End blocklist " << std::endl;

  }
  if (filesize == 0 || !blklist.numBlocks()) {
      checkFileDigest(baseurl, fp, blklist);
      return;
  }
  if (filesize == 0)
    return;

  if (!_multi)
    {
      _multi = curl_multi_init();
      if (!_multi)
        ZYPP_THROW(MediaCurlInitException(baseurl));
    }

  multifetchrequest req(this, filename, baseurl, _multi, fp, report, std::move(blklist), filesize);
  std::vector<Url> myurllist;
  for (std::vector<Url>::iterator urliter = urllist->begin(); urliter != urllist->end(); ++urliter)
    {
      try
        {
          std::string scheme = urliter->getScheme();
          if (scheme == "http" || scheme == "https" || scheme == "ftp" || scheme == "tftp")
            {
              checkProtocol(*urliter);
              myurllist.push_back(internal::propagateQueryParams(*urliter, _url));
            }
        }
      catch (...)
        {
        }
    }
  if (!myurllist.size())
    myurllist.push_back(baseurl);
  req.run(myurllist);
  checkFileDigest(baseurl, fp, req.blockList() );
}

void MediaMultiCurl::checkFileDigest(Url &url, FILE *fp, MediaBlockList &blklist) const
{
  if ( !blklist.haveFileChecksum() )
    return;
  if (fseeko(fp, off_t(0), SEEK_SET))
    ZYPP_THROW(MediaCurlException(url, "fseeko", "seek error"));
  Digest dig;
  blklist.createFileDigest(dig);
  char buf[4096];
  size_t l;
  while ((l = fread(buf, 1, sizeof(buf), fp)) > 0)
    dig.update(buf, l);
  if (!blklist.verifyFileDigest(dig))
    ZYPP_THROW(MediaCurlException(url, "file verification failed", "checksum error"));
}

bool MediaMultiCurl::isDNSok(const std::string &host) const
{
  return _dnsok.find(host) == _dnsok.end() ? false : true;
}

void MediaMultiCurl::setDNSok(const std::string &host) const
{
  _dnsok.insert(host);
}

CURL *MediaMultiCurl::fromEasyPool(const std::string &host) const
{
  if (_easypool.find(host) == _easypool.end())
    return 0;
  CURL *ret = _easypool[host];
  _easypool.erase(host);
  return ret;
}

void MediaMultiCurl::toEasyPool(const std::string &host, CURL *easy) const
{
  CURL *oldeasy = _easypool[host];
  _easypool[host] = easy;
  if (oldeasy)
    curl_easy_cleanup(oldeasy);
}

  } // namespace media
} // namespace zypp
