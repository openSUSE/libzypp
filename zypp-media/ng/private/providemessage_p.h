/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*/
#ifndef ZYPP_MEDIA_PRIVATE_PROVIDE_MESSAGE_P_H_INCLUDED
#define ZYPP_MEDIA_PRIVATE_PROVIDE_MESSAGE_P_H_INCLUDED

#include <zypp-core/rpc/PluginFrame.h>
#include <zypp-core/zyppng/pipelines/expected.h>

/*!
  \internal
  General:
  -------
  This file contains the protocol definitions used in media worker backends. These backends are
  used by zypp mainly to acquire files from different types of media via a generalized API. However,
  it can also be used to implement helpers for special needs, e.g. copying files around or rewriting URLs.

  Every worker type is identified by the schema used in a URL e.g. dvd for getting files from DVDs or https for downloading.
  This makes it possible to easily extend zypp with support for new schemes by implementing a new worker type without the need
  to modify zypp itself. A important functionality is the possibility to redirect URLs to different workers, making it possible
  to implement special schemes that need to remodel a URL in special ways before they are processed by the general backends.

  Communication Format:
  ---------------------
  Each message is serialized into a STOMP frame and sent over the communication medium.
  STOMP is basically a HTTP like protocol, see https://stomp.github.io and \sa zypp::PluginFrame for details.

  Communication channel:
  ----------------------
  Communication between the worker processes and the zypp main process will happen via the standard unix file descriptors:
  stdin, stdout and stderr. stdin and stdout are used to send and receive messages between main and worker processes, while
  stderr is used to output logging that is generated by the worker processes. These logs are forwarded to the main zypp log.

  The workers are supposed to shut down as soon as their stdin is closed. This is important to not block the controller process
  that is waiting for this to happen, to fetch all log lines that are emitted by the worker when shutting down.


  Worker Requirements:
  --------------------
  Async communication:
    Every worker needs to be able to handle messages asynchronously, the frontend will send requests to the worker as
    they are given by the frontend code and does not apply any ordering. This means workers need to remember queries they sent to the frontend and manage
    the code waiting for the result accordingly. The only exception to this is the handshake messages, where the frontend will always
    send the worker configuration first, directly after spawning the worker process and the worker will answer with its capabilities.
    Once those messages have been exchanged, zypp will start to send requests as defined below.
    The frontend will buffer messages sent to the workers, so they do not need to keep the socket empty, however if possible workers should
    support handling multiple requests.

  Cleaning up:
  ------------
    A worker needs to maintain a mountpoint/download location as long as needed. Since only the frontend can decide when a location
    is not required anymore it will send a "ReleaseResource" message with a worker generated ID that releases said ressource.
    Examples for this are mountpoints that a worker created or download areas that were used by the worker to < the files to

  Worker Types:
  -------------
    Currently zypp knows 4 different types of workers:

    * Downloading:      Simplest type of worker that just download the files from a remote location without the need to mount something beforehand.
                        This is the simplest file provider that does not need to implement special functions. It is possible to freely redirect requests
                        between different variations of this worker type, all other types of workers do NOT support redirections.
                        Per remote host a own instance is started except if the SingleInstance flag is set. It is adviceable to set the SingleInstance flag on all
                        workers that do not do any long running requests.

    * Mounting:         This type of worker needs to mount filesystems, but is not restricted by local volatile devices. For example smb, nfs, dir or even disk.
                        It has to additionally implement the Attach/Verify workflow.
                        Mounting handlers are always started as SingleInstance workers.

    * VolatileMounting: Most complex type of workers that mounts local devices that are volatile. This worker needs to also implement the device change workflow.
                        This type of worker exists only to implement the dvd worker.
                        VolatileMounting handlers are started as SingleInstance workers.

    * CPU Bound:        Simple zypp internal workhorse type that can be implemented to run async processes like extracting a compressed file or creating a checksum.
                        For this type of worker a own instance per CPU is started on demand by default.

  Attach/Mount Workflow ( does not apply to Downloading and CPU bound worker types ) :
    Each handler that marks itself as Mounting or VolatileMounting needs to implement the attach/mount workflow. This means that the controller needs to rely
    on the handler to correctly mount and identify a medium. In the Attach message the controller may send media identification data and identification type to the worker
    to specify the exact medium it is looking for. The worker then needs to mount the filesystem and perform the validation check, if the check succeeds the worker
    will assign the unique attach point ID it has received from the contoller to the attachpoint and sends back a AttachResult containing the information.
    At this point the worker needs to keep the mountpoint and attachpoint around until the controller sends a Detach request for it.
    If the verification fails the worker will unmount the filesystem and send back a AttachResult message with the error bits set.
    If the controller does not send verification data, every device/remoteFS that is successfully mounted is considered valid.

    After a medium was mounted the controller will send provide requests to the worker that have URLs formed like:
    <workerscheme>-media://<attachId>/path/to/file. For example: dvd-media://1d6c0810-2bd6-45f3-9890-0268422a6f14/path/to/file , the attachID part is just a example, the controller uses
    generates IDs that can be expressed as a URL hostname.
    Another important aspect to keep in mind, is that a attach point is not the same as a mount point. Since a attach point additionally contains the path relative to the
    filesystem root on the device. For example: the repository drivers is in a subdirectory of a dvd, so we'd get a attach request for the base url: dvd:/drivers, this would
    result in a attachpoint id of "abcdef123" the device would be mounted in "/mnt/device". All subsequent requests are relative to the attachpoint and would look like:
    dvd-media://abcdef123/file , which results in a lookup for the file: "/mnt/device/drivers/file1".

    Due to historical reasons a single device/filesystem can contain multiple mediums. So the worker needs to make sure to not unmount the filesystem too early.
    For example dvd:/repo-1  and dvd:/repo-2 , both mediums are on the same disc, hence the worker will need to maintain two mount IDs and can only release the device itself
    when both have been detached.
    In some cases its even possible that multiple mediums reside in the same (sub)directory on a filesystem. For example when a multi DVD set was copied together into
    one directory shared over NFS.


  Message status codes
  ---------------------
  The response codes are roughtly modelled like HTTP status codes, they can happen during the full runtime of a certain request.

  Informational messages  (100–199)
  Success       messages  (200–299)
  Redirection   messages  (300–399)
  Client error  messages  (400–499)
  Server error  messages  (500–599)
  Controller    messages  (600–699)
  Worker        messages  (700-799)


  - Code: 100 - Provide started
    Desc: Sent to the controller once a worker starts providing a request
          Note that this is a optional message, its perfectly valid if 200 ( Provide Success  ) is sent directly after the file was requested.
    Fields:
      required string url
      string local_filename   -> The local filename where the file will be provided to, used by the controller to track progress
      string staging_filename -> The staging file used while downloading


  - Code: 200 - Provide Finished
    Desc: Sent to the controller once a worker finished a job successfully.
    Fields:
      required string local_filename  -> The path where the worker has placed the file
      required bool   cacheHit        -> Set to true if the file was found in a worker cache


  - Code: 201 - Attach Finished
    Desc: Sent to the controller once a worker mounted a medium successfully.
    Fields:
      optional string localMountPoint  -> Where the device was locally mounted if applicable, e.g. http does not mount

  - Code: 202 - Auth Info
    Desc: Response sent by the controller to a AUTH_REQUIRED request.
          The last_auth_timestamp contains the last change of the auth database. This timestamp should be included
          in successive AUTH_REQUIRED requests, so that the controller knows if its outdated or if the Auth was read from the store once
          and now we need to ask the user to provide new auth info.
    Fields:
      required string username
      required string password
      required int64  auth_timestamp     -> timestamp of the auth data, this should be stored in the worker in case it fails again
      optional string authType           -> comma seperated list of selected auth types, used by the network provider


  - Code: 203 - Media changed
    Desc: Sent to the worker once the user acknowledged a media change request
    Fields:
      - None -

  - Code: 204 - Detach Finished
    Desc: Sent to the controller once a worker detached a medium successfully.
    Fields:
      - None -


  - Code: 300 - Redirect
    Desc: A Downloading worker can generate a Redirect message to reroute a request to a different
          Downloading worker. This can be used to implement special handlers where a URL needs to be rewritten
          before it can actually be fetched.
          This is similar to sending a 200 - Provide Finished message since it will take the request out of the workers queue

          Note: Redirecting is ONLY supported between Downloading workers. Sending this message from other worker types
          or redirecting to non Dowloading workers results in failing the user request with a redirect error.
    Fields:
      required string new_url -> the full URL we want to redirect to


  - Code: 301 - Metalink Redirect
    Desc: A worker can generate a list of mirrors, this is similar to a Redirect response but supports
          giving the Controller a list of possible URLs to try.
          This is similar to sending a 200 - Provide Finished message since it will take the request out of the workers queue.
          The mirror list is ordered by priority, meaning the first mirror has the highest.
    Fields:
      repeated string new_url -> full URL we want to redirect to



  4xx and 5xx messages, all of those messages have the same Response type structure:

  Code: 4xx / 5xx - Client error
  Desc: Sent to the controller for requests that fail due to a client error
  Fields:
  required string reason   -> The reason as a string
  optional string history  -> The error history that lead to the error
  bool   transient         -> If this is set to true, the controller will try to request the file again at a later time

  Currently reserved error codes:
  400: Bad Request            -> invalid request, the receiver can not process it
  401: Unauthorized           -> no auth info avail but auth required
                                 extraFields:  string authHint
  402: Forbidden              -> auth was given but failed or no access rights to the requested resource
                                 extraFields:  string authHint
  403: PeerCertificateInvalid -> the peer certificate validation failed
  404: Not Found              -> the requested resource does not exist on the medium
  405: Expected Size Exceeded -> the downloaded data exceeded the requested maximum lenght
  406: Connection failed      -> connecting to the server failed
  407: Timeout                -> the request timed out
  408: Cancelled              -> request was cancelled by the user
  409: Invalid checksum       -> the downloaded data has a different checksum than expected
  410: Mount failed           -> mounting the requested resource failed
  411: Jammed                 -> ATTACH request failed due to insufficient resources
  412: Media Change Abort     -> User decided to abort the request
  413: Media Change Skip      -> User decided to abort the request with a skip command( request will fail but will report "skipped" as the reason )
  414: No Auth data           -> Only sent by the controller for a 700 - Auth Data Request
  416: Media not desired      -> The desired medium was not found on the given URL ( attach request )

  500: InternalError          -> a error in the worker that is not recoverable, check reason string

  Controller -> Worker requests
  -----------------------------

  - Code: 600 - Provide
    Desc: Message to tell the worker about a ressource it should provide
    Fields:
      required string url -> The URL of the request
      string filename     -> target filename hint, this is not required to be considered by the workers
      string delta_file   -> local path to a file that is supposed to be used for delta downloads
      int64  expected_filesize    -> The expected download filesize, workers should fail if a server does not reports the exact same filesize
      bool   check_existance_only -> this will NOT download the file but only query the server if its existant
      bool   metalink_enabled     -> enables/disables metalink handling

  - Code: 601 - Cancel
    Desc: Sent by the controller if a request should be cancelled. The worker should stop the given request and return a
          408 response.
    Fields:
      - None -

  - Code: 602 - Attach
    Desc:  If a worker signals the frontend it needs to mount/unmount resources the controller will first send a Attach
           message for each base_url it encounters that targets the workers scheme. All workers that do not set this flag
           do not need to care about this special message.
           In fact receiving this message in a worker that is not a mounting type should result
           in returning a error to the controller.

           If the verifyType field is NOT set, the verifyData field will be ignored and the worker does not need to
           verify the device on the given location.
    Fields:
      required string url       -> the base URL we want to attach to
      required string attach_id -> controller generated ID string to uniquely identiy a attached medium
      required string label     -> Label of the medium
      string verify_type        -> name of the verification type, currently only suseV1 ( required if verify_data is present )
      bytes  verify_data        -> verification data ( required if verify_type is present )
      int32 media_nr            -> the nr of the medium in the set ( required if verify_type is present )
      repeated string device    -> optional field containing a device that can be used for attachment

  - Code: 603 - Detach
    Desc: Send by the controller process when a medium is no longer required.
          The worker has to immediately unmount the medium, no answer to the controller is expected.
          Since there is no connected request, the request ID in this message will not be considered
    Fields:
      required string url       -> the attachment URL containing the controller generated ID string to uniquely identify a attached medium, e.g. dvd://<attachId>/


  Worker -> Controller requests
  -----------------------------

  - Code: 700 - Auth Data Request
    Desc: In case a worker needs authorization info it can generate this request.
          The last_auth_timestamp field is present if the request had received auth info before.
    Fields:
      required string effective_url -> the effective URL we want to fetch, this is not necessarily the same as the request URL
      int64 last_auth_timestamp     -> timestamp of previously received auth informationpJuds
      optional string username      -> username tried in the last auth attempt
      optional string authHint      -> comma seperated list of available authentication types
      optional repeated string <keyname> -> all other key:value pairs are treated as extra keys.
    Possible Resposes: 414, 202


  - Code: 701 - Media Change Request
    Desc: Generated by a worker to ask the user to insert a different medium. This will BLOCK the controller and the handler
          until the user answered the request. This is a special request only used by the CDROM handler
    Fields:
      required string label   -> name of the medium we need
      required int32 media_nr -> which medium from the media set is required
      required repeated string device  -> free device to be used to insert the media into
      string desc             -> medium desc
    Possible Resposes: 412, 413, 203

*/

#include <zypp-core/base/PtrTypes.h>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-core/zyppng/rpc/stompframestream.h>
#include <zypp-media/ng/ProvideSpec> // for FieldType
#include <zypp-media/ng/HeaderValueMap>

#include <boost/iterator/iterator_adaptor.hpp>

namespace zypp::proto {
  class Envelope;
  class Configuration;
  class Capabilities;
  class ProvideMessage;
}

namespace zyppng {

  enum MessageCodes : uint32_t {
    NoCode = 0,
    FirstInformalCode = 100,
    ProvideStarted    = 100,
    LastInformalCode  = 199,

    FirstSuccessCode  = 200,
    ProvideFinished   = 200,
    AttachFinished    = 201,
    AuthInfo          = 202,
    MediaChanged      = 203,
    DetachFinished    = 204,
    LastSuccessCode   = 299,

    FirstRedirCode    = 300,
    Redirect          = 300,
    Metalink          = 301,
    LastRedirCode     = 399,

    FirstClientErrCode      = 400,
    BadRequest              = 400,
    Unauthorized            = 401,
    Forbidden               = 402,
    PeerCertificateInvalid  = 403,
    NotFound                = 404,
    ExpectedSizeExceeded    = 405,
    ConnectionFailed        = 406,
    Timeout                 = 407,
    Cancelled               = 408,
    InvalidChecksum         = 409,
    MountFailed             = 410,
    Jammed                  = 411,
    MediaChangeAbort        = 412,
    MediaChangeSkip         = 413,
    NoAuthData              = 414,
    NotAFile                = 415,
    MediumNotDesired        = 416,
    LastClientErrCode       = 499,

    FirstSrvErrCode    = 500,
    InternalError      = 500,
    ProtocolError      = 501,
    LastSrvErrCode     = 599,

    FirstControllerCode = 600,
    Prov                = 600,
    Cancel              = 601,
    Attach              = 602,
    Detach              = 603,
    LastControllerCode  = 699,

    FirstWorkerCode     = 700,
    AuthDataRequest     = 700,
    MediaChangeRequest  = 701,
    LastWorkerCode      = 799,
  };

  class ProviderConfiguration : public std::map<std::string, std::string>
  {
    public:
    using map::map;

    static constexpr std::string_view typeName = "ProviderConfiguration";
    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<ProviderConfiguration> fromStompMessage( const zypp::PluginFrame &msg );
  };

  class WorkerCaps
  {
  public:
    /*! The worker type, see the description in Worker Types above */
    enum WorkerType : uint32_t {
      Invalid       = 0,
      Downloading   = 1,
      SimpleMount   = 2,
      VolatileMount = 3,
      CPUBound      = 4,
    };

    enum Flags : uint32_t {
      None           = 0,   // Just for completeness
      SingleInstance = 1,   // If this flag is set a worker can only be started once, this is implicit in some worker types.
      Pipeline       = 2,   // The worker can handle multiple requests at the same time
      ZyppLogFormat  = 4,   // The worker writes messages to stderr in zypp log format
      FileArtifacts  = 8,   // The results of this worker are artifacts, which means they need to be cleaned up. This is implicit for all downloading workers. For all mounting workers this is ignored.
                            // CPU bound workers can use it to signal they leave artifact files behind that need to be cleaned up
    };

    explicit WorkerCaps();
    ~WorkerCaps();

    WorkerCaps(const WorkerCaps &) = default;
    WorkerCaps(WorkerCaps &&) = default;
    WorkerCaps &operator=(const WorkerCaps &) = default;
    WorkerCaps &operator=(WorkerCaps &&) = default;

    uint32_t protocol_version() const; // The workers should set this field to the protocol version they implement.
    WorkerType worker_type() const;
    Flags  cfg_flags() const;
    const std::string &worker_name() const;

    void set_protocol_version( uint32_t v );
    void set_worker_type( WorkerType t );
    void set_cfg_flags( Flags f );
    void set_worker_name( std::string name );

    static constexpr std::string_view typeName = "WorkerCaps";
    zyppng::expected<zypp::PluginFrame> toStompMessage() const;
    static zyppng::expected<WorkerCaps> fromStompMessage( const zypp::PluginFrame &msg );

  private:
    uint32_t    _protocolVersion = 1;
    uint32_t    _workerType      = Invalid;
    uint32_t    _cfgFlags        = None;
    std::string _workerName;
  };

  namespace ProvideMessageFields {
    constexpr std::string_view RequestCode("requestCode");
    constexpr std::string_view RequestId("requestId");
  }

  namespace ProvideStartedMsgFields
  {
    constexpr std::string_view Url ("url");
    constexpr std::string_view LocalFilename ("local_filename");
    constexpr std::string_view StagingFilename ("staging_filename");
  }

  namespace ProvideFinishedMsgFields
  {
    constexpr std::string_view LocalFilename ("local_filename");
    constexpr std::string_view CacheHit ("cacheHit");
  }

  namespace AuthInfoMsgFields
  {
    constexpr std::string_view Username ("username");
    constexpr std::string_view Password ("password");
    constexpr std::string_view AuthTimestamp ("auth_timestamp");
    constexpr std::string_view AuthType ("authType");
  }

  namespace RedirectMsgFields
  {
    constexpr std::string_view NewUrl ("new_url");
  }

  namespace MetalinkRedirectMsgFields
  {
    constexpr std::string_view NewUrl ("new_url");
  }

  namespace ErrMsgFields
  {
    constexpr std::string_view Reason ("reason");
    constexpr std::string_view Transient ("transient");
    constexpr std::string_view History ("history");
  }

  namespace ProvideMsgFields
  {
    constexpr std::string_view Url ("url");
    constexpr std::string_view Filename ("filename");
    constexpr std::string_view DeltaFile ("delta_file");
    constexpr std::string_view ExpectedFilesize ("expected_filesize");
    constexpr std::string_view CheckExistOnly ("check_existance_only");
    constexpr std::string_view MetalinkEnabled ("metalink_enabled");
  }

  namespace AttachMsgFields
  {
    constexpr std::string_view Url ("url");
    constexpr std::string_view AttachId ("attach_id");
    constexpr std::string_view VerifyType ("verify_type");
    constexpr std::string_view VerifyData ("verify_data");
    constexpr std::string_view MediaNr ("media_nr");
    constexpr std::string_view Device  ("device");
    constexpr std::string_view Label   ("label");
  }

  namespace AttachFinishedMsgFields
  {
    constexpr std::string_view LocalMountPoint ("local_mountpoint");
  }

  namespace DetachMsgFields
  {
    constexpr std::string_view Url ("url");
  }

  namespace AuthDataRequestMsgFields
  {
    constexpr std::string_view EffectiveUrl ("effective_url");
    constexpr std::string_view LastAuthTimestamp ("last_auth_timestamp");
    constexpr std::string_view LastUser ("username");
    constexpr std::string_view AuthHint ("authHint");
  }

  namespace MediaChangeRequestMsgFields
  {
    constexpr std::string_view Label ("label");
    constexpr std::string_view MediaNr ("media_nr");
    constexpr std::string_view Device ("device");
    constexpr std::string_view Desc ("desc");
  }

  namespace EjectMsgFields
  {
    constexpr std::string_view device ("device");
  }

  class ProvideMessage
  {
  public:
    using Code = MessageCodes;

    ProvideMessage(const ProvideMessage &) = default;
    ProvideMessage(ProvideMessage &&) = default;
    ProvideMessage &operator=(const ProvideMessage &) = default;
    ProvideMessage &operator=(ProvideMessage &&) = default;
    using FieldVal = HeaderValue;

    static constexpr std::string_view typeName = "ProvideMessage";

    static expected<ProvideMessage> create ( const zypp::PluginFrame &message );
    expected<zypp::PluginFrame>     toStompMessage() const;
    static expected<ProvideMessage> fromStompMessage( const zypp::PluginFrame &msg );

    static ProvideMessage createProvideStarted  ( const uint32_t reqId, const zypp::Url &url , const std::optional<std::string> &localFilename = {}, const std::optional<std::string> &stagingFilename = {} );
    static ProvideMessage createProvideFinished ( const uint32_t reqId, const std::string &localFilename , bool cacheHit );
    static ProvideMessage createAttachFinished  ( const uint32_t reqId, const std::optional<std::string> &localMountPoint = {} );
    static ProvideMessage createDetachFinished  ( const uint32_t reqId );
    static ProvideMessage createAuthInfo ( const uint32_t reqId, const std::string &user, const std::string &pw, int64_t timestamp, const std::map<std::string, std::string> &extraValues = {} );
    static ProvideMessage createMediaChanged ( const uint32_t reqId );
    static ProvideMessage createRedirect ( const uint32_t reqId, const zypp::Url &newUrl );
    static ProvideMessage createMetalinkRedir ( const uint32_t reqId, const std::vector<zypp::Url> &newUrls );
    static ProvideMessage createErrorResponse (const uint32_t reqId, const Code code, const std::string &reason, bool transient = false  );

    static ProvideMessage createProvide         ( const uint32_t reqId
                                                  , const zypp::Url &url
                                                  , const std::optional<std::string> &filename = {}
                                                  , const std::optional<std::string> &deltaFile = {}
                                                  , const std::optional<int64_t> &expFilesize = {}
                                                  , bool checkExistOnly = false );

    static ProvideMessage createCancel          ( const uint32_t reqId );

    static ProvideMessage createAttach( const uint32_t reqId
                                      , const zypp::Url &url
                                      , const std::string attachId
                                      , const std::string &label
                                      , const std::optional<std::string> &verifyType = {}
                                      , const std::optional<std::string> &verifyData = {}
                                      , const std::optional<int32_t> &mediaNr = {} );

    static ProvideMessage createDetach              ( const uint32_t reqId, const zypp::Url &attachUrl );
    static ProvideMessage createAuthDataRequest     ( const uint32_t reqId, const zypp::Url &effectiveUrl, const std::string &lastTriedUser ="", const std::optional<int64_t> &lastAuthTimestamp = {}, const std::map<std::string, std::string> &extraValues = {} );
    static ProvideMessage createMediaChangeRequest  ( const uint32_t reqId, const std::string &label, int32_t mediaNr, const std::vector<std::string> &devices, const std::optional<std::string> &desc );

    uint requestId () const;
    void setRequestId ( const uint id );

    Code code() const;
    void setCode (const Code newCode );

    const std::vector<ProvideMessage::FieldVal> &values ( const std::string_view &str ) const;
    const std::vector<ProvideMessage::FieldVal> &values ( const std::string &str ) const;
    const HeaderValueMap &headers() const;
    /*!
     * Returns the last entry with key \a str in the list of values
     * or the default value specified in \a defaultVal
     */
    FieldVal value ( const std::string_view &str, const FieldVal &defaultVal = FieldVal() ) const;
    FieldVal value ( const std::string &str, const FieldVal &defaultVal = FieldVal() ) const;
    void setValue  ( const std::string &name, const FieldVal &value );
    void setValue  ( const std::string_view &name, const FieldVal &value );
    void addValue  ( const std::string &name, const FieldVal &value );
    void addValue  ( const std::string_view &name, const FieldVal &value );

  private:
    ProvideMessage();
    uint _reqId = -1;
    Code _code = NoCode;
    HeaderValueMap _headers;
  };
}

#endif
