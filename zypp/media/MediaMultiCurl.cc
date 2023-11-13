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

#include <vector>
#include <iostream>
#include <algorithm>


#include <zypp/ZConfig.h>
#include <zypp/base/Logger.h>
#include <zypp/media/MediaMultiCurl.h>
#include <zypp-curl/parser/MetaLinkParser>
#include <zypp-curl/parser/zsyncparser.h>
#include <zypp/ManagedFile.h>
#include <zypp-curl/private/curlhelper_p.h>
#include <zypp-curl/auth/CurlAuthData>
#include <zypp-curl/parser/metadatahelper.h>

using std::endl;
using namespace zypp::base;

#undef CURLVERSION_AT_LEAST
#define CURLVERSION_AT_LEAST(M,N,O) LIBCURL_VERSION_NUM >= ((((M)<<8)+(N))<<8)+(O)

namespace zypp {
  namespace media {


//////////////////////////////////////////////////////////////////////


class multifetchrequest;

// Hack: we derive from MediaCurl just to get the storage space for
// settings, url, curlerrors and the like

enum MultiFetchWorkerState {
  WORKER_STARTING,
  WORKER_LOOKUP,
  WORKER_FETCH,
  WORKER_DISCARD,
  WORKER_DONE,
  WORKER_SLEEP,
  WORKER_BROKEN
};

class multifetchworker : MediaCurl {
  friend class multifetchrequest;

public:
  multifetchworker(int no, multifetchrequest &request, const Url &url);
  ~multifetchworker();
  void nextjob();
  void run();
  bool checkChecksum();
  bool recheckChecksum();
  void disableCompetition();

  void checkdns();
  void adddnsfd( std::vector<curl_waitfd> &waitFds );
  void dnsevent( const std::vector<curl_waitfd> &waitFds );

  const int _workerno;

  MultiFetchWorkerState _state = WORKER_STARTING;
  bool _competing = false;

  struct BlockData {
    size_t _blkno   = 0;
    off_t _blkstart = 0;
    size_t _blksize = 0;
    std::optional<Digest> _digest;
  };
  std::vector<BlockData> _blocks;

  off_t _datastart = 0; //< The offset we want our download to start from
  size_t _datasize = 0; //< The nr of bytes we need to download overall
  bool _noendrange = false; //< Set to true if we try to request data with no end range, we will cancel the download after we have all bytes?

  double _starttime    = 0; //< When was the current job started
  size_t _datareceived = 0; //< Data downloaded in the current job only
  off_t  _received     = 0; //< Overall data fetched by this worker

  double _avgspeed = 0;
  double _maxspeed = 0;

  double _sleepuntil = 0;

private:
  void stealjob();

  size_t writefunction(void *ptr, size_t size);
  static size_t _writefunction(void *ptr, size_t size, size_t nmemb, void *stream);

  size_t headerfunction(char *ptr, size_t size);
  static size_t _headerfunction(void *ptr, size_t size, size_t nmemb, void *stream);

  multifetchrequest *_request = nullptr;
  int _pass = 0;
  std::string _urlbuf;
  off_t _off   = 0;   //<< Current file offset
  size_t _size = 0; //<< Bytes still to be downloaded

  pid_t _pid   = 0;
  int _dnspipe = -1;
};

class multifetchrequest {
public:
  multifetchrequest(const MediaMultiCurl *context, const Pathname &filename, const Url &baseurl, CURLM *multi, FILE *fp, callback::SendReport<DownloadProgressReport> *report, MediaBlockList *blklist, off_t filesize);
  ~multifetchrequest();

  void run(std::vector<Url> &urllist);

protected:

  static size_t makeBlksize ( size_t filesize );

  friend class multifetchworker;

  const MediaMultiCurl *_context;
  const Pathname _filename;
  Url _baseurl;

  FILE *_fp = nullptr;
  callback::SendReport<DownloadProgressReport> *_report;
  MediaBlockList *_blklist;
  off_t _filesize; //< size of the file we want to download

  CURLM *_multi;

  std::list<multifetchworker *> _workers;
  bool _stealing;
  bool _havenewjob;

  size_t _blkno;
  size_t _defaultBlksize = 0; //< The blocksize to use if the metalink file does not specify one
  off_t _blkoff;
  size_t _activeworkers;
  size_t _lookupworkers;
  size_t _sleepworkers;
  double _minsleepuntil;
  bool _finished;
  off_t _totalsize; //< nr of bytes we need to download ( e.g. filesize - ( bytes reused from deltafile ) )
  off_t _fetchedsize;
  off_t _fetchedgoodsize;

  double _starttime;
  double _lastprogress;

  double _lastperiodstart;
  double _lastperiodfetched;
  double _periodavg;

public:
  double _timeout;
  double _connect_timeout;
  double _maxspeed;
  int _maxworkers;
};

constexpr auto MIN_REQ_MIRRS = 4;
constexpr auto MAXURLS       = 10;

//////////////////////////////////////////////////////////////////////

static double
currentTime()
{
  struct timeval tv;
  if (gettimeofday(&tv, NULL))
    return 0;
  return tv.tv_sec + tv.tv_usec / 1000000.;
}

size_t
multifetchworker::writefunction(void *ptr, size_t size)
{
  size_t len = 0;
  if (_state == WORKER_BROKEN)
    return size ? 0 : 1;

  double now = currentTime();

  len = size > _size ? _size : size;
  if (!len) {
    // kill this job?
    return size;
  }

  // executed the first time writefunction is called
  // just a check if we actually got statuscode 206 for partial data
  if ( _datastart && _off == _datastart )
    {
      // make sure that the server replied with "partial content"
      // for http requests
      char *effurl;
      (void)curl_easy_getinfo(_curl, CURLINFO_EFFECTIVE_URL, &effurl);
      if (effurl && !strncasecmp(effurl, "http", 4))
        {
          long statuscode = 0;
          (void)curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &statuscode);
          if (statuscode != 206)
            return size ? 0 : 1;
        }
    }

  _datareceived += len;
  _received += len;

  _request->_lastprogress = now;

  // blocks are no longer needed, but we can calc checksums for the data we get
  // so bad workers are removed..
  // @TODO shouldn't we simply stop fetching data from the server if we set to discard?
  bool discardData = (_state == WORKER_DISCARD || !_request->_fp);

  // update checksums block by block
  if ( _blocks.size () ) {
    auto i = std::find_if( _blocks.begin(), _blocks.end(), [&]( const BlockData &d ){ return d._blkstart <= _off && d._blkstart + d._blksize > _off; } );
    if ( i == _blocks.end() ) {
      // error
      DBG << "#" << _workerno << "Unable to find current block in given block list" << std::endl;
      return size ? 0 : 1;
    }

    off_t  currOff   = std::max( _off, i->_blkstart );
    size_t remainLen = len;
    size_t bytesWritten = 0;
    while( remainLen > 0 && i != _blocks.end() ) {
      size_t writtenBlk     = 0; // how many bytes have we written in this current iteration
      size_t blockLenRemain = std::min( remainLen, i->_blksize - ( currOff - i->_blkstart ) ); // how many bytes missing in the current block
      if ( !discardData ) {
        // if we can't seek the file there is no purpose in trying again
        if (fseeko(_request->_fp, currOff, SEEK_SET))
          return size ? 0 : 1;

        writtenBlk = fwrite( (const char *)ptr+bytesWritten, 1, blockLenRemain, _request->_fp);
        if ( writtenBlk ) {
          i->_digest->update( (const char *)ptr+bytesWritten, writtenBlk );
        }
      } else {
        writtenBlk += blockLenRemain;
        currOff += blockLenRemain;
        i->_digest->update( (const char *)ptr+bytesWritten, writtenBlk );
      }

      // current data counters
      remainLen    -= writtenBlk;
      currOff      += writtenBlk;
      bytesWritten += writtenBlk;

      // overall data counters
      _off += writtenBlk;
      _size -= writtenBlk;
      _request->_fetchedsize += writtenBlk;

      // if we didn't write the remaining bytes of the block
      // its time to stop
      if ( writtenBlk != blockLenRemain )
          return bytesWritten;

      // next block
      i++;
    }
    return bytesWritten;
  } else {

    size_t cnt = 0;
    if ( discardData ) {
        _off += len;
        _size -= len;
        return size;
    }
    if (fseeko(_request->_fp, _off, SEEK_SET))
      return size ? 0 : 1;
    cnt = fwrite(ptr, 1, len, _request->_fp);
    if (cnt > 0) {
        _request->_fetchedsize += cnt;
        _off += cnt;
        _size -= cnt;
        if (cnt == len)
          return size;
    }
    return cnt;
  }
}

size_t
multifetchworker::_writefunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
  multifetchworker *me = reinterpret_cast<multifetchworker *>(stream);
  return me->writefunction(ptr, size * nmemb);
}

size_t
multifetchworker::headerfunction(char *p, size_t size)
{
  size_t l = size;
  if (l > 9 && !strncasecmp(p, "Location:", 9))
    {
      std::string line(p + 9, l - 9);
      if (line[l - 10] == '\r')
        line.erase(l - 10, 1);
      XXX << "#" << _workerno << ": redirecting to" << line << endl;
      return size;
    }
  if (l <= 14 || l >= 128 || strncasecmp(p, "Content-Range:", 14) != 0)
    return size;
  p += 14;
  l -= 14;
  while (l && (*p == ' ' || *p == '\t'))
    p++, l--;
  if (l < 6 || strncasecmp(p, "bytes", 5))
    return size;
  p += 5;
  l -= 5;
  char buf[128];
  memcpy(buf, p, l);
  buf[l] = 0;
  unsigned long long start, off, filesize;
  if (sscanf(buf, "%llu-%llu/%llu", &start, &off, &filesize) != 3)
    return size;
  if (_request->_filesize == (off_t)-1)
    {
      WAR << "#" << _workerno << ": setting request filesize to " << filesize << endl;
      _request->_filesize = filesize;
      if (_request->_totalsize == 0 && !_request->_blklist)
        _request->_totalsize = filesize;
    }
  if (_request->_filesize != (off_t)filesize)
    {
      XXX << "#" << _workerno << ": filesize mismatch" << endl;
      _state = WORKER_BROKEN;
      strncpy(_curlError, "filesize mismatch", CURL_ERROR_SIZE);
    }
  return size;
}

size_t
multifetchworker::_headerfunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
  multifetchworker *me = reinterpret_cast<multifetchworker *>(stream);
  return me->headerfunction((char *)ptr, size * nmemb);
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
  try
    {
      setupEasy();
    }
  catch (Exception &ex)
    {
      curl_easy_cleanup(_curl);
      _curl = 0;
      _state = WORKER_BROKEN;
      strncpy(_curlError, "curl_easy_setopt failed", CURL_ERROR_SIZE);
      return;
    }
  curl_easy_setopt(_curl, CURLOPT_PRIVATE, this);
  curl_easy_setopt(_curl, CURLOPT_URL, _urlbuf.c_str());
  curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, &_writefunction);
  curl_easy_setopt(_curl, CURLOPT_WRITEDATA, this);
  if (_request->_filesize == off_t(-1) || !_request->_blklist || !_request->_blklist->haveChecksum(0))
    {
      curl_easy_setopt(_curl, CURLOPT_HEADERFUNCTION, &_headerfunction);
      curl_easy_setopt(_curl, CURLOPT_HEADERDATA, this);
    }
  // if this is the same host copy authorization
  // (the host check is also what curl does when doing a redirect)
  // (note also that unauthorized exceptions are thrown with the request host)
  if (url.getHost() == _request->_context->_url.getHost())
    {
      _settings.setUsername(_request->_context->_settings.username());
      _settings.setPassword(_request->_context->_settings.password());
      _settings.setAuthType(_request->_context->_settings.authType());
      if ( _settings.userPassword().size() )
        {
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
  checkdns();
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
multifetchworker::adddnsfd(std::vector<curl_waitfd> &waitFds)
{
  if (_state != WORKER_LOOKUP)
    return;

  waitFds.push_back (
        curl_waitfd {
          .fd = _dnspipe,
          .events = CURL_WAIT_POLLIN,
          .revents = 0
        });
}

void
multifetchworker::dnsevent( const std::vector<curl_waitfd> &waitFds )
{

  bool hasEvent = std::any_of( waitFds.begin (), waitFds.end(),[this]( const curl_waitfd &waitfd ){
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

bool
multifetchworker::checkChecksum()
{
  // XXX << "checkChecksum block " << _blkno << endl;
  if (!_datasize || !_request->_blklist || !_blocks.size() )
    return true;
  return std::all_of ( _blocks.begin (), _blocks.end(), [&]( BlockData &block ){
    if ( block._digest && !_request->_blklist->verifyDigest( block._blkno, *block._digest ) ) {
      WAR << "#" << _workerno << ": Block: " << block._blkno << " failed to verify checksum" << endl;
      return false;
    }
    return true;
  });
}

bool
multifetchworker::recheckChecksum()
{
  // XXX << "recheckChecksum block " << _blkno << endl;
  if (!_request->_fp || !_datasize || !_request->_blklist || !_blocks.size() )
    return true;

  for ( auto &blk : _blocks ) {
    if ( !_request->_blklist->haveChecksum ( blk._blkno ) )
      continue;

    if (fseeko(_request->_fp, blk._blkstart, SEEK_SET))
      return false;

    blk._digest.emplace();
    _request->_blklist->createDigest( *blk._digest );	// resets digest

    char buf[4096];
    size_t l = blk._blksize;
    while (l) {
      size_t cnt = l > sizeof(buf) ? sizeof(buf) : l;
      if (fread(buf, cnt, 1, _request->_fp) != 1)
        return false;
      blk._digest->update(buf, cnt);
      l -= cnt;
    }

    if ( !_request->_blklist->verifyDigest( blk._blkno, *blk._digest ) ) {
      WAR << "#" << _workerno << ": Block: " << blk._blkno << " failed to verify checksum" << endl;
      return false;
    }
  }
  return true;
}


void multifetchworker::stealjob()
{
  if (!_request->_stealing)
    {
      XXX << "start stealing!" << endl;
      _request->_stealing = true;
    }
  multifetchworker *best = 0;
  std::list<multifetchworker *>::iterator workeriter = _request->_workers.begin();
  double now = 0;
  for (; workeriter != _request->_workers.end(); ++workeriter)
    {
      multifetchworker *worker = *workeriter;
      if (worker == this)
        continue;
      if (worker->_pass == -1)
        continue;	// do not steal!
      if (worker->_state == WORKER_DISCARD || worker->_state == WORKER_DONE || worker->_state == WORKER_SLEEP || !worker->_datasize)
        continue;	// do not steal finished jobs
      if (!worker->_avgspeed && worker->_datareceived)
        {
          if (!now)
            now = currentTime();
          if (now > worker->_starttime)
            worker->_avgspeed = worker->_datareceived / (now - worker->_starttime);
        }
      if (!best || best->_pass > worker->_pass)
        {
          best = worker;
          continue;
        }
      if (best->_pass < worker->_pass)
        continue;
      // if it is the same block, we want to know the best worker, otherwise the worst
      if (worker->_datastart == best->_datastart)
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
      if (_avgspeed && best->_avgspeed && best->_datasize - best->_datareceived > 0 &&
          (best->_datasize - best->_datareceived) * _avgspeed < best->_datasize * best->_avgspeed)
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
  _datastart = best->_datastart;
  _datasize  = best->_datasize;
  _blocks.clear ();
  std::for_each( best->_blocks.begin(), best->_blocks.end(), [&](BlockData &b) {
    _blocks.emplace_back( BlockData{
      ._blkno    = b._blkno,
      ._blkstart = b._blkstart,
      ._blksize  = b._blksize
    });
    if ( _request->_blklist->haveChecksum( b._blkno) ) {
      zypp::Digest d;
      _request->_blklist->createDigest( d );
      _blocks.back()._digest = std::move(d);
    }
  });
  best->_pass++;
  _pass = best->_pass;
  run();
}

void
multifetchworker::disableCompetition()
{
  std::list<multifetchworker *>::iterator workeriter = _request->_workers.begin();
  for (; workeriter != _request->_workers.end(); ++workeriter)
    {
      multifetchworker *worker = *workeriter;
      if (worker == this)
        continue;
      if (worker->_datastart == _datastart )
        {
          if (worker->_state == WORKER_FETCH)
            worker->_state = WORKER_DISCARD;
          worker->_pass = -1;	/* do not steal this one, we already have it */
        }
    }
}


/*!
 * Gets the next n blocks from the request, the way we handle this is to
 * increase blocknr in the next request by as many blocks we take until it reaches the blockcount.
 * That way we know when all blocks have been taken by a worker.
 *
 * IFF downloading of blocks fails, another worker might steal the job later on. We will fail either all
 * blocks or none.
 */
void multifetchworker::nextjob()
{
  _datasize  = 0;
  _datastart = -1;
  _blocks.clear();

  _noendrange = false;
  if (_request->_stealing)
    {
      stealjob();
      return;
    }

  MediaBlockList *blklist = _request->_blklist;
  if (!blklist)
    {
      // if we have no blocklist we just download chunks in the size of _defaultBlksize
      // in theory also supports having no filesize
      _datasize = _request->_defaultBlksize;
      if (_request->_filesize != off_t(-1))
      {
        if (_request->_blkoff >= _request->_filesize)
          {
            stealjob();
            return;
          }
        _datasize = _request->_filesize - _request->_blkoff;
        if (_datasize > _request->_defaultBlksize)
          _datasize = _request->_defaultBlksize;
      }
      DBG << "No BLOCKLIST falling back to chunk size: " <<  _request->_defaultBlksize << std::endl;
    }
  else
    {
      MediaBlock blk = blklist->getBlock(_request->_blkno);
      // find the next block to download
      while (_request->_blkoff >= (off_t)(blk.off + blk.size)) {
          // we hit the end of the list
          if (++_request->_blkno == blklist->numBlocks()) {
              stealjob();
              return;
          }
          blk = blklist->getBlock(_request->_blkno);
          _request->_blkoff = blk.off;
      }

      DBG << "#" << _workerno << "Collecting blocks to download" << std::endl;
      const auto &addBlock = [&]( bool isFirst = false ) {
        // XXX << "#" << _workerno << "Adding block nr " << _request->_blkno << " to range" << std::endl;
        _blocks.emplace_back( BlockData{
                                ._blkno    = _request->_blkno,
                                ._blkstart = blk.off,
                                ._blksize  = blk.size,
                                ._digest   = {}
                              } );

        // in case the first block we find is not exactly aligned with the blockoffset
        // unlikely but the original code did this so we continue to do so
        if ( isFirst )
          _datasize = blk.off + blk.size - _request->_blkoff;
        else
          _datasize += blk.size;

      };

      // push the first block in
      addBlock( true );


      // pick as many blocks we need until we reach _defaultBlkSize
      // or until we find a gap between blocks
      while ( _datasize < _request->_defaultBlksize && ( _request->_blkno + 1 ) < _request->_blklist->numBlocks ()) {
        blk = blklist->getBlock( _request->_blkno + 1 );
        if ( _blocks.back ()._blkstart + _blocks.back ()._blksize < blk.off )
          break;

        // consecutive blocks, and we have bytes left lets take it
        ++_request->_blkno;
        addBlock();
      }

      DBG << "#" << _workerno << "Done adding blocks to download, going to download " << _blocks.size() << " nr of block with " << _datasize << " nr of bytes" << std::endl;

    }
  _datastart = _request->_blkoff;
  _request->_blkoff += _datasize;
  run();
}

void
multifetchworker::run()
{
  char rangebuf[128];

  if (_state == WORKER_BROKEN || _state == WORKER_DONE)
     return;	// just in case...
  if (_noendrange)
    sprintf(rangebuf, "%llu-", (unsigned long long)_datastart);
  else
    sprintf(rangebuf, "%llu-%llu", (unsigned long long)_datastart, (unsigned long long)_datastart + _datasize - 1);
  XXX << "#" << _workerno << ":" << rangebuf << " " << _url << endl;
  if (curl_easy_setopt(_curl, CURLOPT_RANGE, !_noendrange || _datastart != 0 ? rangebuf : (char *)0) != CURLE_OK)
    {
      _request->_activeworkers--;
      _state = WORKER_BROKEN;
      strncpy(_curlError, "curl_easy_setopt range failed", CURL_ERROR_SIZE);
      return;
    }
  if (curl_multi_add_handle(_request->_multi, _curl) != CURLM_OK)
    {
      _request->_activeworkers--;
      _state = WORKER_BROKEN;
      strncpy(_curlError, "curl_multi_add_handle failed", CURL_ERROR_SIZE);
      return;
    }

  _request->_havenewjob = true;
  _off  = _datastart;
  _size = _datasize;

  // resets digests
  if ( _request->_blklist && _blocks.size() ) {
    for ( auto &b : _blocks ) {
      if ( _request->_blklist->haveChecksum ( b._blkno ) ) {
        b._digest.emplace();
        _request->_blklist->createDigest( *b._digest );
      } else {
        b._digest.reset();
      }
    }
  }

  _state = WORKER_FETCH;

  double now = currentTime();
  _starttime = now;
  _datareceived = 0;
}


//////////////////////////////////////////////////////////////////////


multifetchrequest::multifetchrequest(const MediaMultiCurl *context, const Pathname &filename, const Url &baseurl, CURLM *multi, FILE *fp, callback::SendReport<DownloadProgressReport> *report, MediaBlockList *blklist, off_t filesize) : _context(context), _filename(filename), _baseurl(baseurl)
{
  _fp = fp;
  _report = report;
  _blklist = blklist;
  _filesize = filesize;
  _defaultBlksize = makeBlksize( filesize );
  _multi = multi;
  _stealing = false;
  _havenewjob = false;
  _blkno = 0;
  if (_blklist)
    _blkoff = _blklist->getBlock(0).off;
  else
    _blkoff = 0;
  _activeworkers = 0;
  _lookupworkers = 0;
  _sleepworkers = 0;
  _minsleepuntil = 0;
  _finished = false;
  _fetchedsize = 0;
  _fetchedgoodsize = 0;
  _totalsize = 0;
  _lastperiodstart = _lastprogress = _starttime = currentTime();
  _lastperiodfetched = 0;
  _periodavg = 0;
  _timeout = 0;
  _connect_timeout = 0;
  _maxspeed = 0;
  _maxworkers = 0;
  if (blklist)
    {
      for (size_t blkno = 0; blkno < blklist->numBlocks(); blkno++)
        {
          MediaBlock blk = blklist->getBlock(blkno);
          _totalsize += blk.size;
        }
    }
  else if (filesize != off_t(-1))
    _totalsize = filesize;
}

multifetchrequest::~multifetchrequest()
{
  for (std::list<multifetchworker *>::iterator workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
    {
      multifetchworker *worker = *workeriter;
      *workeriter = NULL;
      delete worker;
    }
  _workers.clear();
}

void
multifetchrequest::run(std::vector<Url> &urllist)
{
  int workerno = 0;
  std::vector<Url>::iterator urliter = urllist.begin();
  for (;;)
    {
      // our custom fd's we want to poll
      std::vector<curl_waitfd> extraWaitFds;

      if (_finished)
        {
          XXX << "finished!" << endl;
          break;
        }

      if ((int)_activeworkers < _maxworkers && urliter != urllist.end() && _workers.size() < MAXURLS)
        {
          // spawn another worker!
          multifetchworker *worker = new multifetchworker(workerno++, *this, *urliter);
          _workers.push_back(worker);
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
          for (std::list<multifetchworker *>::iterator workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
            {
              if ((*workeriter)->_state != WORKER_BROKEN)
                continue;
              ZYPP_THROW(MediaCurlException(_baseurl, "Server error", (*workeriter)->_curlError));
            }
          break;
        }

      if (_lookupworkers)
        for (std::list<multifetchworker *>::iterator workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
          (*workeriter)->adddnsfd( extraWaitFds );

      // if we added a new job we have to call multi_perform once
      // to make it show up in the fd set. do not sleep in this case.
      int timeoutMs = _havenewjob ? 0 : 200;
      if (_sleepworkers && !_havenewjob) {
        if (_minsleepuntil == 0) {
          for (std::list<multifetchworker *>::iterator workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter) {
            multifetchworker *worker = *workeriter;
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

      int r = 0;
#if CURLVERSION_AT_LEAST(7,66,0)
      CURLMcode mcode = curl_multi_poll( _multi, extraWaitFds.data(), extraWaitFds.size(), timeoutMs, &r );
#else
      CURLMcode mcode = curl_multi_wait( _multi, extraWaitFds.data(), extraWaitFds.size(), timeoutMs, &r );
#endif
      if ( mcode != CURLM_OK ) {
        ZYPP_THROW(MediaCurlException(_baseurl, "curl_multi_poll() failed", str::Str() << "curl_multi_poll() returned CurlMcode: " << mcode ));
      }
      if (r == -1 && errno != EINTR)
        ZYPP_THROW(MediaCurlException(_baseurl, "curl_multi_poll() failed", "unknown error"));
      if (r != 0 && _lookupworkers)
        for (std::list<multifetchworker *>::iterator workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
          {
            multifetchworker *worker = *workeriter;
            if (worker->_state != WORKER_LOOKUP)
              continue;
            (*workeriter)->dnsevent( extraWaitFds );
            if (worker->_state != WORKER_LOOKUP)
              _lookupworkers--;
          }
      _havenewjob = false;

      // run curl
      for (;;)
        {
          CURLMcode mcode;
          int tasks;
          mcode = curl_multi_perform(_multi, &tasks);
          if (mcode == CURLM_CALL_MULTI_PERFORM)
            continue;
          if (mcode != CURLM_OK)
            ZYPP_THROW(MediaCurlException(_baseurl, "curl_multi_perform", "unknown error"));
          break;
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
          for (std::list<multifetchworker *>::iterator workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
            {
              multifetchworker *worker = *workeriter;
              if (worker->_state != WORKER_SLEEP)
                continue;
              if (worker->_sleepuntil > now)
                continue;
              if (_minsleepuntil == worker->_sleepuntil)
                _minsleepuntil = 0;
              XXX << "#" << worker->_workerno << ": sleep done, wake up" << endl;
              _sleepworkers--;
              // nextjob chnages the state
              worker->nextjob();
            }
        }

      // collect all curl results, reschedule new jobs
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
          if (worker->_datareceived && now > worker->_starttime)
            {
              if (worker->_avgspeed)
                worker->_avgspeed = (worker->_avgspeed + worker->_datareceived / (now - worker->_starttime)) / 2;
              else
                worker->_avgspeed = worker->_datareceived / (now - worker->_starttime);
            }
          XXX << "#" << worker->_workerno << " done code " << cc << " speed " << worker->_avgspeed << endl;
          curl_multi_remove_handle(_multi, easy);
          if (cc == CURLE_HTTP_RETURNED_ERROR)
            {
              long statuscode = 0;
              (void)curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &statuscode);
              XXX << "HTTP status " << statuscode << endl;
              if (statuscode == 416 && !_blklist)	/* Range error */
                {
                  if (_filesize == off_t(-1))
                    {
                      if (!worker->_noendrange)
                        {
                          XXX << "#" << worker->_workerno << ": retrying with no end range" << endl;
                          worker->_noendrange = true;
                          worker->run();
                          continue;
                        }
                      worker->_noendrange = false;
                      worker->stealjob();
                      continue;
                    }
                  if (worker->_datastart >= _filesize)
                    {
                      worker->nextjob();
                      continue;
                    }
                }
            }
          if (cc == 0)
            {
              if (!worker->checkChecksum())
                {
                  WAR << "#" << worker->_workerno << ": checksum error, disable worker" << endl;
                  worker->_state = WORKER_BROKEN;
                  strncpy(worker->_curlError, "checksum error", CURL_ERROR_SIZE);
                  _activeworkers--;
                  continue;
                }
              if (worker->_state == WORKER_FETCH)
                {
                  if (worker->_competing)
                    {
                      worker->disableCompetition();
                      // multiple workers wrote into this block. We already know that our
                      // data was correct, but maybe some other worker overwrote our data
                      // with something broken. Thus we have to re-check the block.
                      if (!worker->recheckChecksum())
                        {
                          XXX << "#" << worker->_workerno << ": recheck checksum error, refetch block" << endl;
                          // re-fetch! No need to worry about the bad workers,
                          // they will now be set to DISCARD. At the end of their block
                          // they will notice that they wrote bad data and go into BROKEN.
                          worker->run();
                          continue;
                        }
                    }
                  _fetchedgoodsize += worker->_datasize;
                }

              // make bad workers sleep a little
              double maxavg = 0;
              int maxworkerno = 0;
              int numbetter = 0;
              for (std::list<multifetchworker *>::iterator workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
                {
                  multifetchworker *oworker = *workeriter;
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
          else
            {
              worker->_state = WORKER_BROKEN;
              _activeworkers--;
              if (!_activeworkers && !(urliter != urllist.end() && _workers.size() < MAXURLS))
                {
                  // end of workers reached! goodbye!
                  worker->evaluateCurlCode(Pathname(), cc, false);
                }
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
  for (std::list<multifetchworker *>::iterator workeriter = _workers.begin(); workeriter != _workers.end(); ++workeriter)
    {
      multifetchworker *worker = *workeriter;
      WAR << "#" << worker->_workerno << ": state: " << worker->_state << " received: " << worker->_received << " url: " << worker->_url << endl;
    }
}

inline size_t multifetchrequest::makeBlksize ( size_t filesize )
{
  // this case should never happen because we never start a multi download if we do not know the filesize beforehand
  if ( filesize == 0 )  return 2 * 1024 * 1024;
  else if ( filesize < 2*256*1024 ) return filesize;
  else if ( filesize < 8*1024*1024 ) return 256*1024;
  else if ( filesize < 256*1024*1024 ) return 1024*1024;
  return 4*1024*1024;
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
            XXX << "With " << bl.numBlocks() << " nr of blocks and a blocksize of " << bl.getBlock(0).size << std::endl;
          } else {
            MetaLinkParser mlp;
            mlp.parse(destNew);
            bl = mlp.getBlockList();
            urls = mlp.getUrls();

            XXX << getFileUrl(srcFile.filename()) << " returned metalink meta data." << std::endl;
            XXX << "With " << bl.numBlocks() << " nr of blocks and a blocksize of " << bl.getBlock(0).size << std::endl;
          }

          /*
           * gihub issue libzipp:#277 Multicurl backend breaks with MirrorCache and Metalink with unknown filesize.
           * Fall back to a normal download if we have no knowledge about the filesize we want to download.
           */
          if ( !bl.haveFilesize() && ! srcFile.downloadSize() ) {
            XXX << "No filesize in metalink file and no expected filesize, aborting multicurl." << std::endl;
            ZYPP_THROW( MediaException("Multicurl requires filesize but none was provided.") );
          }

          /*
           * bsc#1191609 In certain locations we do not receive a suitable number of metalink mirrors, and might even
           * download chunks serially from one and the same server. In those cases we need to fall back to a normal download.
           */
          if ( urls.size() < MIN_REQ_MIRRS ) {
            ZYPP_THROW( MediaException("Multicurl enabled but not enough mirrors provided") );
          }

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
              multifetch(srcFile.filename(), file, &urls, &report, &bl, srcFile.downloadSize());
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

void MediaMultiCurl::multifetch(const Pathname & filename, FILE *fp, std::vector<Url> *urllist, callback::SendReport<DownloadProgressReport> *report, MediaBlockList *blklist, off_t filesize) const
{
  Url baseurl(getFileUrl(filename));
  if (blklist && filesize == off_t(-1) && blklist->haveFilesize())
    filesize = blklist->getFilesize();
  if (blklist && !blklist->haveBlocks() && filesize != 0)
    blklist = 0;
  if (blklist && (filesize == 0 || !blklist->numBlocks()))
    {
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

  multifetchrequest req(this, filename, baseurl, _multi, fp, report, blklist, filesize);
  req._timeout = _settings.timeout();
  req._connect_timeout = _settings.connectTimeout();
  req._maxspeed = _settings.maxDownloadSpeed();
  req._maxworkers = _settings.maxConcurrentConnections();
  if (req._maxworkers > MAXURLS)
    req._maxworkers = MAXURLS;
  if (req._maxworkers <= 0)
    req._maxworkers = 1;
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
  checkFileDigest(baseurl, fp, blklist);
}

void MediaMultiCurl::checkFileDigest(Url &url, FILE *fp, MediaBlockList *blklist) const
{
  if (!blklist || !blklist->haveFileChecksum())
    return;
  if (fseeko(fp, off_t(0), SEEK_SET))
    ZYPP_THROW(MediaCurlException(url, "fseeko", "seek error"));
  Digest dig;
  blklist->createFileDigest(dig);
  char buf[4096];
  size_t l;
  while ((l = fread(buf, 1, sizeof(buf), fp)) > 0)
    dig.update(buf, l);
  if (!blklist->verifyFileDigest(dig))
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
