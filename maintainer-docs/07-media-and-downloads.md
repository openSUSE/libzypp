# 7. Media Access and Downloads

This chapter covers how libzypp gets bytes from a repository URL onto the local disk. It describes the legacy
`MediaManager`/`MediaHandler` stack, the HTTP backends (`MediaCurl`, `MediaCurl2`), mirrors (`MirroredOrigin`),
multi-medium sets (`MediaSetAccess`), the curl and proxy helpers, the NG network stack (`NetworkRequest*`), and the NG
`Provide` worker framework. It also says which code path runs by default today.

> Scope note: the repo-level handling of mirrorlist and metalink files (`RepoMirrorList`, `RepoInfo`) belongs to
> [Repositories](05-repositories-and-services.md). The keys in `zypp.conf` are covered in full in
> [Configuration](09-configuration-and-environment.md). The NG architecture as a whole is covered in [zyppng](10-zyppng-next-generation.md).

---

## 7.1 The big picture: what is used today

Many older docs, blog posts and changelog entries mention `MediaAccess`, `MediaMultiCurl`, `MediaNetwork` and
`ZYPP_MEDIANETWORK`/`ZYPP_MULTICURL`. **None of these exist in the current tree any more:**

| Historic component | Status (verified in tree) | Changelog |
|---|---|---|
| `MediaAccess` (per-handler wrapper owned by MediaManager) | Removed. `MediaManager_Impl` now holds a `ManagedMedia` that owns a `std::unique_ptr<MediaHandler>` directly (`zypp/zypp/media/MediaManager.cc`). Comments still say "see MediaAccess". | – |
| `MediaNetwork` (tech preview, used the NG `Downloader` in-process) | Removed ("Remove Medianetwork and dependend code", 2022). `ZConfig` still turns `techpreview.ZYPP_MEDIANETWORK` into a `setenv("ZYPP_MEDIANETWORK")` (`zypp/zypp/ZConfig.cc:530`), and `zypp/tools/DownloadFiles.cc` still sets it, but **nothing reads it**. | 17.31.x |
| `MediaMultiCurl` (metalink/zsync, parallel block download from mirrors) | Removed ("Drop MediaMultiCurl backend", 17.37.0). `?mediahandler=multicurl` / `network` now quietly maps to the default. | 17.37.0 |
| `zyppng::Downloader` / `MirrorControl` (NG in-process downloader with metalink) | Removed from `zypp-logic`. Only the tests `zypp/tests/zyppng/media/EvDownloader_test.cc` still mention it. Changelog 17.38.0: "zyppng: Update Provider to current MediaCurl2 download approach, drop Metalink (fixes #682)". | 17.38.0 |
| `MediaCurl` (classic curl_easy-on-multi backend) | **Still present**. It is the default only when built with `LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT` (the spec does this for `suse_version < 1600`, i.e. Code15/SLE15/Leap 15). | – |
| `MediaCurl2` (uses NG `NetworkRequestDispatcher` synchronously) | **Default HTTP/FTP backend** for Code16+ / Tumbleweed builds. Introduced in 17.36.3, made the default for Code16 in 17.37.0. | 17.36.3, 17.37.0 |
| NG `Provide` + `zypp-media-*` worker processes | Built only when the NG tree is built (`BUILD_LIBZYPPNG`; spec `%bcond_with build_libzyppng`, which is off by default and forced off for `<1600`). Installed to `${CMAKE_INSTALL_LIBEXECDIR}/zyppng/workers`. **Legacy libzypp (zypper/YaST/PackageKit) never spawns workers.** | – |

The default legacy data flow:

```
 zypper / YaST / PackageKit
        │
 RepoManager / PackageProvider / Fetcher / RepoMediaAccess  (see ch. 5, 8)
        │
 MediaSetAccess  ── one MediaAccessId per medium number; media change callbacks
        │
 MediaManager  (envelope around a process-wide static MediaManager_Impl)
        │  open(MirroredOrigin) → MediaHandlerFactory::createHandler()
        ▼
 MediaHandler subclasses:
   cd/dvd → MediaCD   nfs/nfs4 → MediaNFS   iso → MediaISO   file/dir → MediaDIR
   hd → MediaDISK     cifs/smb → MediaCIFS  plugin → (resolved via UrlResolverPlugin, else error)
   http/https/ftp/tftp → MediaNetworkCommonHandler
                           ├─ MediaCurl2 (default)  → internal::MediaNetworkRequestExecutor
                           │                          → zyppng::NetworkRequestDispatcher (curl multi, in-process)
                           └─ MediaCurl  (legacy)   → curl_easy handle inside private curl_multi + zypp_poll
```

There is also a *legacy* class called `zyppng::Provide` in `zypp/zypp/ng/media/provide.{h,cc}` (header guard
`ZYPP_NG_MEDIASETFACADE_INCLUDED`). It is a **synchronous facade over `MediaManager`** used by the legacy-side
"ng" workflows (`zypp/zypp/ng/repo/*`). It is not the worker-based `Provide` in `zyppng/lib/zypp-media/ng/provide.cc`.
The two share a name but nothing else. Check which one you have open before you change anything.

---

## 7.2 Selecting the HTTP backend

`MediaHandlerFactory::createHandler()` (`zypp/zypp/media/MediaHandlerFactory.cc`) decides:

1. If the **authority** URL has the query parameter `mediahandler`: `curl` → MediaCurl, `curl2` → MediaCurl2. The old values
   `network` and `multicurl` → "choose default". Any other value logs a warning and uses the default.
2. If no choice was made yet: env `ZYPP_CURL2` (read with `env::getenvBool`): `1` → MediaCurl2, `0` → MediaCurl.
3. If the env var is unset: the compile-time default. `APIConfig(LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT)` → MediaCurl,
   otherwise MediaCurl2. The CMake option is in `zypp/CMakeLists.txt:34` (default OFF) and becomes
   `zypp-logic/zypp-core/APIConfig.h.in`. `zypp/libzypp.spec.cmake` turns it ON for `suse_version < 1600`,
   together with `LIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT` (that one controls the
   `CommitPackagePreloader`; env `ZYPP_PCK_PRELOAD`, see [Target/commit](08-target-commit-and-rpm.md)).

`clearQueryString()` strips `mediahandler` before the request (it is listed twice in the strip list, which does no harm).
Changelog 17.36.4 also strips it from baseUrl query strings when they are stored.

---

## 7.3 MediaManager, ManagedMedia and access ids

File: `zypp/zypp/media/MediaManager.{h,cc}`. The doxygen header in `MediaManager.h` is the **user-facing reference
for all URL schemes and query parameters**. Keep it current when you change URL options. Note that it still says
`mediahandler` values are `curl`/`multicurl`, which is out of date.

* `MediaManager` is a cheap envelope. All instances share `static RW_pointer<MediaManager_Impl> m_impl`, created on first
  construction. The header warns: **never declare a static `MediaManager`** (initialization order problems).
* `MediaAccessId` is a plain `unsigned` counter (`nextAccessId()`). It is never reused within a process.
* `ManagedMedia` holds `desired` (cached verifier result), `verifier` (`MediaVerifierRef`, default `NoVerifier`),
  `deltafile` (set via `setDeltafile()`, used for zchunk), and the handler.
* `open(MirroredOrigin, preferred_attach_point)` creates the handler. It does **not** attach.
* `attach(id)` first tries `handler.attach(false)` and `checkDesired`. If that fails and `hasMoreDevices()` (CD/DVD with
  several drives), it loops over `attach(true)` until a drive holds the desired medium.
* Every `provideFile/provideDir/precacheFiles/setDeltafile` call goes through `checkDesired()`. `dirInfo` and `doesFileExist`
  only call `checkAttached()`; a `FIXME` there asks whether they should check `desired` too.
* `close(id)` throws `MediaIsSharedException` if some other handler `dependsOnParent(id)`. This is the ISO-on-NFS case
  (see 7.6).
* `releaseAll()` runs **two passes**: first handlers that depend on a parent (ISO loop mounts), then the rest
  (bsc#1196061 "Fix handling of ISO media in releaseAll"). `~MediaManager_Impl` also erases dependent handlers first.
* `isChangeable(id)` is simply `scheme == cd || dvd`.
* `isUseableAttachPoint(path, mtab)` rejects `/`, a non-directory, a path that is already in use, and a path that has a
  mount point below it. Unless `mtab=false`, it checks against `Mount::getEntries()` (`/proc/mounts` / mtab) as well.

### MediaVerifier

`MediaVerifierBase::isDesiredMedia(const MediaHandler&)` is the hook that checks whether the attached medium is the one
that was asked for. For SUSE media this means `media.1/media` (vendor, ID, media number). The concrete verifier lives
on the repo side (`repo::SUSEMediaVerifier`, see chapter 5). `MediaSetAccess::setVerifier(nr, v)` stores it until the
medium is opened. Changelog history: "MediaVerifier: Relax media set verification in case of a single not-volatile
medium (bsc#1180851)" and "Fix unsafe usage of static in media verifier".

---

## 7.4 MediaHandler: attach points and the provide/release contract

File: `zypp/zypp/media/MediaHandler.{h,cc}` (log group `zypp::media`). This is a template-method base class. The public
non-virtual methods do the checks and logging. Subclasses implement:

| virtual | meaning |
|---|---|
| `attachTo(bool next)` | mount, connect, or prepare. `next=true` means "try the next device" (CD only; network handlers throw `MediaNotSupportedException`). |
| `releaseFrom(ejectDev)` | unmount (and eject if requested) |
| `disconnectFrom()` | drop connections, keep the attach point |
| `getFile`, `getFileCopy`, `getDir`, `getDirInfo`, `getDoesFileExist` | provide content below `localRoot()` |
| `forceEject`, `hasMoreDevices`, `getDetectedDevices`, `precacheFiles` | optional |

Key members: `_origin` (a `MirroredOrigin`), `_attachPoint` (`AttachPointRef`, shared), `_attachPointHint`, `_relativeRoot`
(the URL path below the attach point), `_does_download` (if true, `releasePath` **deletes** the local copies),
`_mediaSource` (`MediaSourceRef`), and `_parentId`.

### Attach point creation (`createAttachPoint()`)

The lookup order is: the explicit `MediaManager::setAttachPrefix()` → `ZConfig::download_mediaMountdir()` (zypp.conf
`download.media_mountdir`, default `/var/adm/mount`) → `TmpPath::defaultLocation()`. It creates `AP_0x??????` temp dirs
with `filesystem::TmpDir` and `autoCleanup(false)`. **Once per process**, it removes orphaned `AP_0x*` dirs on the same device
that are older than a month. If no writable directory is found, it throws `MediaBadAttachPointException`.

For network handlers the attach point is just a **download cache directory**. Files land in
`attachPoint/<path-in-url>/…` and are moved or copied away by the caller (`Fetcher`, `PackageProvider`). `MediaCurl`/`MediaCurl2`
constructors probe the hinted attach point with `mkdtemp`. If that fails they fall back to a temporary one.

### Shared media and mount table checks

* A `MediaSource` (`zypp/zypp/media/MediaSource.h`) describes the *thing* that is mounted (type, name, major/minor, bind dir).
  Two handlers that attach the same source share the `MediaSourceRef` and `AttachPointRef`. `isSharedMedia()` is
  `!_mediaSource.unique()`.
* `release()` only calls `releaseFrom` (umount) if the source is unique. If it is shared and an eject is requested, it calls
  `MediaManager::forceReleaseShared()` first.
* `checkAttached(matchMountFs)` re-reads the mount table only when its mtime changed. It contains several quirks:
  nfs ↔ nfs4 and cifs ↔ smb are treated as equal (bnc#710269). NFS names are not compared because the server resolves symlinks
  (bnc#804544). For ISO, a `/dev/loopN` source in mtab is accepted. If a medium disappears from mtab it logs "forcing reset" and
  drops `_mediaSource`.
* `localPath()` = `localRoot() + pathname.absolutename()`. Network `getFile` uses `.absolutename()` so that `..` cannot
  escape the attach point. Security-relevant; see also the CVEs in the 17.38.x changelog for repo-supplied paths.

### Directory listings: `directory.yast`

`getDirInfo` for network media (and for local media too, because `NONREMOTE_DIRECTORY_YAST` is 1) downloads
`<dir>/directory.yast` and parses it. A trailing `/` marks a directory. `provideDirTree` over HTTP therefore
**only works if the server has `directory.yast` files**.

---

## 7.5 Mirrors in the media layer: `MirroredOrigin`

Files: `zypp-logic/zypp-core/MirroredOrigin.{h,cc}`, test `zypp/tests/zypp/MirroredOrigin_test.cc`.

* `OriginEndpoint` = `Url` + a `std::any` config map (`hasConfig/getConfig<T>/setConfig/eraseConfigValue`). This map is how
  per-endpoint state is carried around: `"http-headers"` (`UrlResolverPlugin::HeaderList`), and
  `MediaNetworkCommonHandler::MIRR_SETTINGS_KEY = "_zypp_transfersettings"` (a fully computed `TransferSettings`).
* `MirroredOrigin` = one or more **authorities** plus mirrors. Multiple authorities were added in 17.38.6 (bsc#1253193).
  Index space: `[0, authorityCount())` are authorities, the rest are mirrors. `_origin[i]` indexes across both.
* `MirroredOriginSet` groups several origins (e.g. baseurls with different schemes) and supports fallback iteration
  (`hasFallbackUrls()`).

How the handler side uses it:

* `MediaHandlerFactory::createHandler` resolves every endpoint through `UrlResolverPlugin::resolveUrl` and takes the
  handler type from the **first authority**. **Mirrors with a different handler type are dropped silently** (`MIL`
  "Different handler type than primary URL, ignoring"). Non-curl handlers with more than one endpoint log an `ERR` and ignore the mirrors.
* `MediaNetworkCommonHandler` ctor: the default `_mirrOrder` is **mirrors first, then authorities**.
* `mirrorOrder(loc)`: if `!loc.mirrorsAllowed()` (security-relevant files such as `repomd.xml`, keys and signatures are
  marked by the repo code; 17.37.6 "means to disable the use of mirrors when downloading security relevant files"),
  it returns only the authorities. `getDoesFileExist` always uses `setMirrorsAllowed(false)`.
* `deprioritizeMirror(idx)` rotates a failing endpoint to the end of `_mirrOrder`. This persists for the lifetime of the
  handler (openSUSE/zypper#636, 17.38.5).
* `canTryNextMirror(excpt)`: the fatal exceptions are `MediaRequestCancelledException`, `MediaWriteException`,
  `MediaSystemException`, `MediaCurlInitException` and `MediaUnauthorizedException`. Every other exception (including 404) moves on to
  the next endpoint.
* `MediaSetAccess::provide`: if the user changes the URL in the media-change callback, **all mirrors are dropped**
  (`clearMirrors()`).

### GeoIP redirect

`MediaNetworkCommonHandler::findGeoIPRedirect()` checks for `ZConfig::geoipCachePath()/<hostname>` (the file
holds a replacement host, filled during refresh, see chapter 5). This is disabled by `ZConfig::geoipEnabled()==false`
(`download.use_geoip_mirror`) or by a `COUNTRY`/`AVOID_COUNTRY` query parameter. `getFileUrl()` never redirects
files that match `^.*/repomd.xml(.asc|.key)?$` or `^/geoip$`.

> Bug to be aware of: `getFileUrl()` checks `mirrorIdx > _redirTargets.size()`. It should be `>=`. All callers pass
> valid indices today, so it is harmless.

---

## 7.6 The concrete non-network handlers (short reference)

| Handler | File | Notes |
|---|---|---|
| `MediaCD` | `zypp/zypp/media/MediaCD.cc` | Detects drives via **libudev** (`subsystem=block`, `ID_CDROM=1`, and `ID_CDROM_DVD` for `dvd:`). The `?devices=` URL option overrides detection (bnc#755815). Prefers `/dev/dvd` or `/dev/cdrom`. Ejects via ioctl in `forceEject`/`releaseFrom`. `REPORT_EJECT_ERRORS` is 0, so eject errors are ignored. `hasMoreDevices()`/`getDetectedDevices()` feed the media-change dialog. |
| `MediaDISK` | `MediaDISK.cc` | `hd:/sub?device=/dev/sda1&filesystem=auto`. Mounts the block device. SEGV fix bsc#1245452. |
| `MediaDIR` | `MediaDIR.cc` | `dir:` / `file:`. No mount. `localRoot` is the directory itself (`_does_download=false`), so `releaseFile` never deletes anything. |
| `MediaISO` | `MediaISO.cc` | `iso:/sub?iso=X.iso&url=nfs://…&mnt=…&filesystem=udf`. Opens a **parent** handler through `MediaManager` for `url=` (`_parentId`) and loop-mounts the iso. That is why `dependsOnParent`, two-pass release and `MediaIsSharedException` exist. Encoding of iso: URLs fixed in bsc#954813. |
| `MediaNFS` | `MediaNFS.cc` | `nfs`, `nfs4`, `?type=nfs4`, `?mountoptions=` (default `ro`). |
| `MediaCIFS` | `MediaCIFS.cc` | `cifs`/`smb` are the same (mount type is always `cifs`). `ro` is added unless `rw` is given; `guest` is added only when no credentials are known (`noguest` suppresses it). Credentials come from the URL authority, from `username=`/`user=`/`password=`/`pass=` **inside `?mountoptions=`** (not separate query params), or from `CredentialManager`; `?workgroup=`/`?domain=` → `domain=`. Details in 7.19. |
| `MediaPlugin` | `MediaPlugin.cc` | **Stub.** `plugin:` must be rewritten to a real scheme by `UrlResolverPlugin` (`/usr/lib/zypp/plugins/urlresolver/`). If not, `MediaUnsupportedUrlSchemeException` is thrown (bsc#1228208). |

Mounting is done through `zypp-logic/zypp-media/mount.{h,cc}` (`Mount::mount/umount` call `/bin/mount` and `/bin/umount` via
`ExternalProgram`; umount is retried when the device is busy, bsc#1196061). CD helpers are in `zypp-logic/zypp-media/cdtools.cc`.

`MediaPriority` (`zypp/zypp/media/MediaPriority.{h,cc}`) maps a URL scheme to a small integer. Its only production
user is `sat::detail::PoolImpl::setRepoInfo()`, which stores it as the libsolv **repo subpriority** (tie-breaker between
repos with the same `priority`). It does not pick a handler and does not order baseurls. Full table in 7.20.
`nfs4` was added in commit 7d1e05a6c.

---

## 7.7 Network handlers in detail

### Common base: `MediaNetworkCommonHandler`

`zypp/zypp/media/MediaNetworkCommonHandler.{h,cc}`:

* `attachTo()` creates an attach point if needed, calls `disconnectFrom()`, then **`setupTransferSettings()` for every
  endpoint**, then sets a `MediaSource(scheme, url)`.
* `setupTransferSettings()` does the following for each endpoint, in this order:
  1. A fresh `TransferSettings` (defaults come from `MediaConfig`, see 7.9).
  2. `checkProtocol()`. MediaCurl uses curl's protocol list; MediaCurl2 uses `NetworkRequestDispatcher::supportsProtocol`.
  3. `setUserAgentString(agentString())` = `"ZYpp <ver> (curl <ver>) <targetDistribution>"`, **trimmed** (bsc#1212187:
     HTTP/2 rejects header values that end with a space. This caused the infamous "curl error 92 HTTP/2 PROTOCOL_ERROR").
  4. `http-headers` from the endpoint config (set by `UrlResolverPlugin` or by `RepoInfo` host-specific headers, bsc#1268321).
  5. For host `download.opensuse.org` only: `X-ZYpp-AnonymousId` and `X-ZYpp-DistributionFlavor` (bsc#955801).
  6. `Pragma:` (an empty header that removes curl's default).
  7. `::internal::fillSettingsFromUrl()` (the legacy query options, 7.8).
  8. If the proxy is still empty, `fillSettingsSystemProxy()`.
  9. bsc#1174011: with `auth=basic`, a username in the URL and no password, the password is preloaded from `CredentialManager`.
* `authenticate()` (static, also used by `CommitPackagePreloader`) tries stored credentials once (`firstTry`) and then asks the user
  via `AuthenticationReport::prompt`. New credentials are saved through `CredentialManager::addCred` + `save()`.
  **bsc#1210870**: callers must pass `firstTry=false` after the first round, otherwise wrong stored credentials loop forever.

### `MediaCurl2` (default)

`zypp/zypp/media/MediaCurl2.cc`. For each endpoint in `mirrorOrder(srcFile)`:

1. Download to a temp file `target.new.zypp.XXXXXX` (`mkostemp`, O_CLOEXEC; the `ManagedFile` unlinks it on failure).
2. Build a `zyppng::NetworkRequest(clearQueryString(fileurl), tmp, WriteShared)`, copy that endpoint's `TransferSettings`,
   and call `setExpectedFileSize(srcFile.downloadSize())`.
3. Try zchunk first if this is an `ENABLE_ZCHUNK_COMPRESSION` build and `srcFile.deltafile()` is a zck file (`tryZchunk` → `ZckLoader`,
   which adds range requests with per-chunk digests). Otherwise run a plain request.
4. Apply `chmod(applyUmaskTo(0644))` (bsc#1243887: cached packages 0644 & ~umask), then rename into place.
5. On a `MediaException`: if it is fatal or this was the last endpoint, rethrow. A `MediaFileNotFoundException` is rewritten
   to carry the authority URL and the filename. Otherwise deprioritize the endpoint and continue.

`getDoesFileExist` sends a `HeadRequest` to `/dev/null` and tries authorities only. Credential handling in HEAD requests was
fixed in bsc#1244105.

**Not implemented in MediaCurl2** (see the `#if 0` blocks and the TODO at the top of the file): `If-Modified-Since`, an overall
request timeout, metalink and zsync. Per-file mirror and block download from metalink is gone. Mirrors now come only from
repo-level `mirrorlist`/`metalink` (`RepoMirrorList`) and become `MirroredOrigin` mirrors.

`internal::MediaNetworkRequestExecutor` (`zypp/zypp/media/detail/MediaNetworkRequestExecutor.cc`) is the synchronous
bridge. It creates a private `EventLoop`, enqueues the request on its own `NetworkRequestDispatcher`, and runs the loop until
`sigFinished`. Then it maps `NetworkRequestError::Type` to legacy exceptions:

| NG error | legacy exception | report code |
|---|---|---|
| NotFound | `MediaFileNotFoundException` | NOT_FOUND |
| Unauthorized / AuthFailed | emits `sigAuthRequired` and retries; otherwise `MediaUnauthorizedException` | ACCESS_DENIED |
| Cancelled | `MediaRequestCancelledException` | ERROR |
| ExceededMaxLen | `MediaFileSizeExceededException` | ERROR |
| TemporaryProblem / Timeout / Forbidden | `MediaTemporaryProblemException` / `MediaTimeoutException` / `MediaForbiddenException` | ERROR |
| everything else, incl. Http2Error | `MediaCurlException` | ERROR |

Retry: **any** error (also 404 and cancel) is retried while `maxSilentTries-1 > 0`. The default `download.max_silent_tries` is 1,
so there is no extra retry by default. Raising it multiplies the 404 probes per mirror.
Progress goes through `internal::ProgressTracker` (`detail/DownloadProgressTracker.h`). If the user returns false from
`progress()`, the request is cancelled.

### `MediaCurl` (legacy, Code15 default)

`zypp/zypp/media/MediaCurl.cc`. It uses one `curl_easy` handle per file, added to a private `curl_multi`. The loop is driven by
`internal::CurlPollHelper` + `zypp_detail::zypp_poll()`, so that a shutdown request can cancel media operations
("Add a way to cancel media operations on shutdown", "Manually poll fds for curl in MediaCurl"). `setupEasy()` sets the following
curl options:

* `FAILONERROR`, `NOSIGNAL`, `IPRESOLVE` from `ZYPP_MEDIA_CURL_IPRESOLVE` (4/6), `CONNECTTIMEOUT`, and `TIMEOUT=3600` as a hard cap
  when a transfer timeout is set. The real no-data timeout is computed in `ProgressData` (`now - _timeRcv > timeout`).
* `FOLLOWLOCATION` with `MAXREDIRS 6` (bnc#465532). Redirects are restricted to **https only** via
  `setCurlRedirProtocols()`. This is detected at runtime for curl ≥ 7.85 (`CURLOPT_REDIR_PROTOCOLS_STR`, bsc#1218831).
* SSL: `CAPATH`, `SSLCERT`, `SSLKEY`, `CURLSSLOPT_ALLOW_BEAST` (bnc#779177), `VERIFYPEER`/`VERIFYHOST(2)`,
  `SSLVERSION=TLSv1` (minimum TLS1.x, bnc#903405 POODLE).
* Proxy and `PROXYUSERPWD`. If no proxy user is given, `proxy-user` is read from `~/.curlrc` (`CurlConfig`). With `_none_`,
  `NOPROXY="*"` is set. Also `PROXY_TRANSFER_MODE` (bnc#306272).
* `LOW_SPEED_LIMIT` = min_download_speed over 60 s, and `MAX_RECV_SPEED_LARGE`.
* Cookie file `/var/lib/YaST2/cookies` (mode 0600). It is read only if `?cookies=` is true, but **always** written as the jar.
* `If-Modified-Since` on existing targets; `304` or FTP `213` → "not modified" and the target is kept. bnc#692260: a future
  date can cause a 200 with an empty body, which is detected by `CURLINFO_CONDITION_UNMET` and retried without the condition.
* `evaluateCurlCode`: 401 → Unauthorized (with an auth hint from `CURLINFO_HTTPAUTH_AVAIL`); 502/503 → TemporaryProblem
  (bnc#1070851, bnc#462545); 504 → Timeout; 403 → Forbidden with an SCC hint for `*.suse.com`; 404/410 → FileNotFound.
* The expected size is enforced in `ProgressData::writeBytes` → `MediaFileSizeExceededException`. Related bugs: bsc#1245220
  (do not trigger on HTTP non-data responses) and bsc#1233393 (the 20 MB metadata limit must not apply to package URLs).
* `MediaCurlExceptionMayRetryInternaly` (meant to retry HTTP/2 errors up to 3 times) is **never thrown any more**. The
  retry branch in `getFileCopyFromMirror` is dead code.
* `doGetDoesFileExist`: uses `HEAD` for http(s) unless `?head_requests=no`, in which case it uses `Range: 0-1` (also for ftp).

### Download progress callbacks

Both backends wrap the receiver in `internal::OptionalDownloadProgressReport` (`detail/OptionalDownloadProgressReport.cc`):

* For optional files, `start()` is delayed until the first data arrives. `finish/problem` are suppressed if nothing started.
  This avoids "file not found" noise for optional metadata (#416).
* `progress()` is throttled to at most one call per 100 ms (bsc#1239543).

---

## 7.8 URL query options and curl/proxy helpers (`zypp-logic/zypp-curl`)

`zypp-logic/zypp-curl/curlhelper.cc` + `private/curlhelper_p.h` (the namespace is `::internal`, so it is private API):

| Query option | Effect (`fillSettingsFromUrl`) |
|---|---|
| `timeout` | 0..3600 s; out-of-range values are ignored |
| user/pass in authority | `setUsername/Password`. ftp without a user → `anonymous` / `zypp@<version>` |
| `ssl_verify` | https only: empty/`yes` → host+peer, `no` → none, `host,peer` list. An unknown flag throws `MediaBadUrlException` |
| `ssl_capath` | must be an absolute existing dir (default `/etc/ssl/certs`) |
| `ssl_clientcert`, `ssl_clientkey` | must be absolute existing files. bsc#1226030: ssl_clientkey is stripped from stored repo URLs |
| `proxy`, `proxyport`, `proxyuser`, `proxypass` | explicit proxy. `proxy=_none_` (`EXPLICITLY_NO_PROXY`) disables the proxy even if the system has one |
| `auth` | basic, digest, ntlm, negotiate, spnego, gssnego (checked by `CurlAuthData::auth_type_str2long`). An invalid value → `MediaUnauthorizedException`. ftp → `basic` preset |
| `head_requests=no` | workaround for broken servers |
| `cookies` | enable the cookie file |
| `mediahandler`, `credentials`, `username`, `password` | consumed elsewhere and stripped by `clearQueryString` |

`clearQueryString()` removes these options, the user, the password, path params and the fragment before the URL goes to curl.
**Remember**: if you add a new option, add it to the strip list too, or it will be sent to the server.
`propagateQueryParams()` copies proxy and ssl params from the repo URL to derived URLs (bsc#933839, boo#1127591).

TransferSettings (`zypp-logic/zypp-curl/transfersettings.{h,cc}`) is a COW pimpl. Note that **the `Impl` defaults
`verify_host/peer` to false**. Verification is switched on only by `fillSettingsFromUrl` for `https`. Code that builds
`TransferSettings` by hand (tests, tools, NG workers) must do this itself. `addHeader` and `setUserAgentString` trim their input and
drop empty headers (bsc#1212187). `logCredentials`/`logUserPass` mask passwords in logs.

### Proxy resolution (`ProxyInfo`)

* `ProxyInfo()` uses `ProxyInfoLibproxy` if `libproxy.so.1` can be **dlopen'ed at runtime** (jsc#PED-8153, "Dynamically resolve
  libproxy"; libproxy is optional and no longer required on Tumbleweed). Otherwise it falls back to `ProxyInfoSysconfig("proxy")`,
  which reads `/etc/sysconfig/proxy` (`PROXY_ENABLED`, `HTTP(S)_PROXY`, `FTP_PROXY`, `NO_PROXY`).
* Libproxy factory: rebuilt when `/etc/sysconfig/proxy` changes (`WatchFile`). It temporarily unsets `KDE_FULL_SESSION`,
  `GNOME_DESKTOP_SESSION_ID` and `DESKTOP_SESSION` so that libproxy does not consult desktop settings.
* Result selection (bsc#1244710, bsc#1247690): only `http*` entries count. Outside YaST the **first** one wins. Under YaST
  (`YAST_IS_RUNNING`) the **last** one wins, because libproxy lists env proxies before sysconfig and YaST wants sysconfig edits to
  take effect at once. `PX_DEBUG=1` logs all libproxy answers in log group `PX_DEBUG`.
* `useProxyFor()`: `NO_PROXY="*"` disables the proxy. Otherwise a domain-suffix match is used (a leading `.` is ignored).
* If no proxy is configured anywhere, libzypp passes nothing and libcurl may still use `http_proxy` etc. from the environment.

`CurlConfig::parseConfig` (`curlconfig.cc`) reads `$HOME/.curlrc` only if the dir and file are readable by the process.
It only evaluates `proxy-user`.

---

## 7.9 Download settings (`zypp.conf`) and env vars

`MediaConfig` (`zypp-logic/zypp-media/mediaconfig.cc`) is a singleton fed by `ZConfig` with the `[main]` keys it knows
(`setConfigValue`):

| key | default | used by |
|---|---|---|
| `download.max_concurrent_connections` | 5 | `CommitPackagePreloader` dispatcher only (`zypp/zypp/target/commitpackagepreloader.cc:457`). MediaCurl/MediaCurl2 download one file at a time. The `NetworkRequestDispatcher` default without this setting is 10. |
| `download.min_download_speed` | 0 (off) | `CURLOPT_LOW_SPEED_LIMIT` (60 s window) |
| `download.max_download_speed` | 0 (off) | `CURLOPT_MAX_RECV_SPEED_LARGE` |
| `download.max_silent_tries` | 1 | `MediaNetworkRequestExecutor` retry count |
| `download.transfer_timeout` | 180, clamped 0..3600 | no-data timeout (bsc#1199042: monitored from the start, not from the first data) |
| `download.connect_timeout` | 60, min 0 | `CURLOPT_CONNECTTIMEOUT` (bsc#1208329) |
| `credentials.global.dir` / `.file` | `/etc/zypp/credentials.d`, `/etc/zypp/credentials.cat` | `CredentialManager` |

These are read by `ZConfig` itself rather than `MediaConfig`: `download.media_mountdir` (`/var/adm/mount`),
`download.use_deltarpm` (+ `.always`; the default depends on `LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT`, OFF since 17.36.3 except
SLE ≤15.6 via the spec), and `download.use_geoip_mirror`.

| Env var | Effect | Where |
|---|---|---|
| `ZYPP_CURL2=0/1` | pick the HTTP backend | `MediaHandlerFactory.cc` |
| `ZYPP_PCK_PRELOAD=0/1` | parallel package preload during commit | `commitpackagepreloader.cc` |
| `ZYPP_MEDIA_CURL_DEBUG=N` | 1: curl info text, 2: + headers (the `Authorization` value is masked), 3: + byte counts of data/SSL, 4: hexdump. Also enables `L_*_MEDIA` debug logs in the NG network code (`ng/network/private/mediadebug_p.h`) and zchunk block logs at >3 | `curlhelper.cc`, NG dispatcher |
| `ZYPP_MEDIA_CURL_IPRESOLVE=4/6` | force IPv4/IPv6 (logged as WAR) | MediaCurl and NG `request.cc` |
| `ZYPP_METALINK_DEBUG=1` | log mirrors and priorities parsed from metalink/zsync | `parser/metalinkparser.cc`, `zsyncparser.cc` |
| `PX_DEBUG=1` | log libproxy results | `proxyinfolibproxy.cc` |
| `ZYPP_MEDIA_PROVIDER_DEBUG` | verbose NG Provide logging (NG only) | `zyppng/lib/zypp-media/ng/provide.cc` |
| `ZYPP_MEDIANETWORK`, `ZYPP_MULTICURL` | **dead**: still set by ZConfig techpreview / DownloadFiles, read by nothing | – |

---

## 7.10 MediaSetAccess: multi-medium sets and media change

`zypp/zypp/MediaSetAccess.{h,cc}`, test `zypp/tests/zypp/MediaSetAccess_test.cc` with data in `zypp/tests/zypp/data/mediasetaccess`.

* It holds one `MirroredOrigin` (for medium numbers > 1, `getMediaAccessId` rewrites every endpoint with
  `MediaSetAccess::rewriteUrl(url, nr)`, which uses a regex to swap the trailing number in `(cd|dvd|media)N` paths or `(cd|dvd|media)N.iso` iso names), a label, a
  map from medium number to `MediaAccessId`, and the pending verifiers.
* `provideFile(OnMediaLocation, ProvideFileOptions)`: the `medianr` comes from the location. `PROVIDE_NON_INTERACTIVE` is
  implied for optional files (`provideOptionalFile`).
* The core loop is `provide(op, resource, options)`:
  1. `getMediaAccessId(nr)`, attach if not attached, run `op`.
  2. On a `MediaException`, map it to a `MediaChangeReport::Error`: FileNotFound/NotAFile → `NOT_FOUND`;
     NotDesired/NotAttached → `WRONG`; Timeout/TemporaryProblem → `IO_SOFT`; anything else → `INVALID`.
  3. If **no `MediaChangeReport` receiver is connected**, or the call is non-interactive and the reason is not `WRONG`, rethrow the
     original exception.
  4. Otherwise call `releaseAll()` first (#336881), then `report->requestMedia(url, nr, label, reason, history, devices, devindex)`.
     - `ABORT` → `AbortRequestException`, `IGNORE` → `SkipRequestException` (both remember the original).
     - `EJECT` → `releaseAll()` + `release(media, devices[devindex])`, then ask again.
     - `RETRY`/`CHANGE_URL` → close the access id, erase it, and loop (a new handler is opened). A changed URL drops all mirrors.
* `doesFileExist` is implemented as a `provide` with a probing op. `release()` releases all its media ids.
* Changelog: bsc#1211661 "Do not unconditionally release a medium if provideFile failed". 17.31.x "Skip media.1/media download
  for http repo status calc".

---

## 7.11 NG network stack (`zypp-logic/zypp-curl/ng/network`)

This code lives in `zypp-logic` because legacy `MediaCurl2` and the preloader use it as well as NG. It is under `ng/`, so it may change
(AGENTS.md tier 2), but legacy callers must keep compiling. This section is an overview. The internals (dispatcher, request
state machine, range batching, zchunk, error mapping) are covered in 7.15, and the parsers in 7.18.

* `zyppng::NetworkRequest` (`request.{h,cc}`): URL, target file, `TransferSettings`, options (`HeadRequest`, `WriteShared`, …),
  expected size, and **range requests** (`addRequestRange(start, len, digest, checksum, userdata, relevantDigestLen, chksumPad)`,
  `resetRequestRanges`). Per-range digests give block checksum verification (formerly multicurl, now used by zchunk). Multi-range
  HTTP responses are parsed by `CurlMultiPartHandler` (`curlmultiparthandler.cc`). If the server refuses a multi-range request it
  retries with **smaller batches** (255 → 127 → 63 → 15 → 5 ranges per request); it never auto-downgrades to one range per request
  over HTTP (see 7.15). Signals: `sigStarted`, `sigProgress`, `sigBytesDownloaded`, `sigFinished`. It also does redirect protocol
  restriction (https URLs only), `ZYPP_MEDIA_CURL_IPRESOLVE` and `_none_` proxy handling, the same as MediaCurl.
* `NetworkRequestDispatcher`: a curl multi handle driven by the zypp `EventDispatcher` (glib-based), with a queue and a
  `_maxConnections` limit (default 10). `supportsProtocol(url)`.
* `NetworkRequestError` (`networkrequesterror.{h,cc}`): typed errors plus `extraInfo` (e.g. `authHint`).
* `zckhelper.{h,cc}` (`ZckLoader`): reads the zck header of the target and the local deltafile, computes the missing chunks
  (`sigBlocksRequired`), and verifies the result. `PrepareResult::{Success, NothingToDo, Error, ExceedsMaxLen}`.
* Parsers (`zypp-logic/zypp-curl/parser/`): `MetaLinkParser` (metalink v3/v4 → URLs with priorities and an optional
  `MediaBlockList`), `ZsyncParser`, and `MediaBlockList` (block checksums; `tools/CalculateReusableBlocks.cc`). Their **only production
  user** today is `zypp/zypp/repo/RepoMirrorList.cc`, which turns a repo-level metalink into a mirror URL list. Format detection: if the
  file starts with `<` it is treated as metalink. URL variables inside mirrorlist/metalink files are replaced (#667).
* `HttpHeader` (`zypp-logic/zypp-curl/HttpHeader`, `ng/network/httpheader.h`): a name/value pair, used for host-specific headers
  (bsc#1268321) and the anonymous-id/flavor headers.

---

## 7.12 NG Provide framework and media workers (NG only)

Files: `zyppng/lib/zypp-media/ng/{provide,providequeue,provideitem,provideres,providemessage}.cc`, `private/*_p.h`,
the worker base `zyppng/lib/zypp-media/ng/worker/provideworker.{h,cc}` and `devicedriver.h`, the shared specs in
`zypp-logic/zypp-media/ng/{providespec,headervaluemap,lazymediahandle}.h`, and the workers in `zyppng/tools/zypp-media-*`.

Concept: media access runs **out of process**. `Provide` keeps queues of `ProvideItem`s (file requests, attach requests). It
starts worker executables `workerPath()/zypp-media-<scheme>` (`providequeue.cc:70`). The default path is the compile-time
`ZYPP_WORKER_PATH`, `${libexecdir}/zyppng/workers`, and `Provide::setWorkerPath()` overrides it (tests). Messages go over the
worker's stdin/stdout. They are `ProvideMessage` objects (a `HeaderValueMap` with typed fields), framed by the RPC layer.

```
 Provide (scheduler) ──┬─ ProvideQueue[http host A] ── zypp-media-http  (Downloading queue: several workers per host)
                       ├─ ProvideQueue[disc]        ── zypp-media-disc  (SimpleMount/VolatileMount: attach/detach)
                       └─ ProvideQueue[chksum/copy] ── zypp-media-chksum / zypp-media-copy (CPU-bound helpers)
```

* The worker capabilities (type, flags) are queried first ("Failed to query scheme config"). Downloading queues reject `Attach`
  requests, and CPU-bound queues reject attach specs (`provide.cc:232`, `:454`). The scheduler balances requests between existing
  workers, idle workers and mirrors that have no connection yet (`provide.cc` ~238-320).
* Message codes (`private/providemessage_p.h`) look like HTTP: 1xx informal (`ProvideStarted`), 2xx success (`ProvideFinished`,
  `AttachFinished`, `AuthInfo`, `MediaChanged`, `DetachFinished`), 3xx `Redirect`, 4xx client errors (`Unauthorized`,
  `NotFound`, `ExpectedSizeExceeded`, `MountFailed`, `Jammed`, `MediaChangeAbort/Skip`, `NoAuthData`, `MediumNotDesired`, …),
  5xx worker errors, and 6xx controller requests (`Prov`, `Cancel`, `Attach`, `Detach`). An int overflow in the message parser was fixed
  in #559.
* Config pushed to workers: `zconfig://media/UserAgent`, `DistributionFlavor`, `AnonymousId`, `AttachPoint`, `ProviderRoot`
  (`provide-configvars.h`). The per-request key `zypp-req-specific-devices` limits a request to certain devices.
* Workers: `http` (`NetworkProvider`, uses `NetworkRequestDispatcher` and follows the MediaCurl2 approach since 17.38.0, without metalink),
  `disc` (udev + eject), `disk`, `dir`, `iso`, `nfs`, `smb`, `copy`, `chksum`, and `tvm` (a test VM fake device used by
  `zyppng/tests/media/Provider_test.cc`).
* Packaging: the workers are built only with `BUILD_LIBZYPPNG` (the NG and zypp-glib tree). The legacy `libzypp` package does not install
  them, and nothing in `zypp/` spawns them.

### Migration status (legacy backends → NG providers)

1. **Done**: MediaNetwork and MediaMultiCurl are gone. Legacy HTTP downloads use the NG `NetworkRequest` in-process via
   MediaCurl2 by default on Code16 and Tumbleweed. The preloader uses the NG dispatcher for parallel package downloads.
2. **Still legacy**: all mount-based handlers (CD/DVD, ISO, NFS, CIFS, disk, dir), `MediaManager`, `MediaSetAccess` and the media-change
   UI callbacks. On Code15 builds, MediaCurl is still the HTTP default.
3. **NG target**: `zyppng::Provide` with worker processes replaces `MediaManager` in the NG application tier
   (`zyppng/`). Per AGENTS.md, legacy stays frozen. Do not port legacy handlers to workers inside `zypp/`. New behaviour goes
   into the workers and `zyppng/lib/zypp-media/ng/`.

---

## 7.13 RPC layer (`zypp-logic/zypp-core/rpc`)

There is **one wire format** (STOMP 1.2-like frames) with **two parsers**:

| Piece | File | Role |
|---|---|---|
| `zypp::PluginFrame` | `zypp-logic/zypp-core/rpc/PluginFrame.{h,cc}` | The frame value type (COW pimpl): command, header multimap, `ByteArray` body. `writeTo(ostream)` serialises; `PluginFrame(istream)` is a **blocking** parser. `PluginFrameException` for parse errors. Predefined commands `ACK`, `ERROR`, `_ENOMETHOD`. |
| `zyppng::StompFrameStream` | `zypp-logic/zypp-core/ng/rpc/stompframestream.{h,cc}` | **Non-blocking**, event-loop driven parser/writer on top of a `zyppng::IODevice` (pipe, socket, process). Queues complete frames, emits `sigMessageReceived` (re-emitted via a 0 ms timer while the queue is non-empty), `sigInvalidMessageReceived`. `nextMessage(name)`, `nextMessageWait(name)` (blocks with `waitForReadyRead`), `sendFrame`, `sendMessage<T>`. |
| `rpc::toStompMessage/fromStompMessage`, `prepareFrame<T>()`, `parseHeaderIntoField` | `stompframestream.h` | Typed messages: a struct with `static constexpr std::string_view typeName` (= STOMP command) plus `toStompMessage()`/`static fromStompMessage()`. |

Frame layout as written by `PluginFrame::Impl::writeTo`:

```
COMMAND\n
content-length:<body bytes>\n        ← always written first, regenerated on every write
key:value\n                          ← header names/values escaped: \n→\\n, \r→\\r, \\→\\\\, ':'→\\c (names only)
...
\n                                   ← empty line ends headers
<body bytes>\0                       ← NUL terminates the frame
```

* Values are **not** colon-escaped (bsc#1231043: python `zypp-plugin` cannot unescape `\c`; the first `:` splits key/value
  anyway). Unknown escape sequences are a fatal parse error (`unescapeHeader`).
* Readers honour `content-length` (read exactly N bytes + `\0`, then strip the header) or, without it, read up to the first `\0`.
  python `zypp-plugin` < 0.6.4 rejected the `content-length` header ("headers including a '-'"), hence the package conflict
  (bsc#1227793).
* `StompFrameStream` limits: command line 256 B, header line 8 KiB, body without `content-length` 1 MiB (the limit for bodies *with*
  `content-length` is `#if 0`'ed). On error it enters `ParseError` and skips to the next `\0`.
* The two parsers must stay in sync. Both files carry an "ATTENTION: Remember to also update..." comment.

Who talks this protocol:

| Peer | Transport | Parser used | Messages |
|---|---|---|---|
| Legacy plugins: `PluginScript` (`zypp/zypp/PluginScript.cc`) used by `UrlResolverPlugin`, `PluginExecutor` (commit/system plugins), `repo/PluginSigcheck` | child stdin/stdout, `g_poll` with timeouts (`ZYPP_PLUGIN_TIMEOUT`, `_SEND_TIMEOUT`, `_RECEIVE_TIMEOUT`, default 30 s) | blocking `PluginFrame(istream)` | free-form commands, `_DISCONNECT` on close; `ZYPP_PLUGIN_DEBUG` dumps buffers |
| `zypp-rpm` helper (`TargetImpl.cc` ~2786 ↔ `zypp/tools/zypp-rpm/main.cc`) | dedicated message pipe + child stdin | `StompFrameStream` | typed structs in `zypp-logic/shared/commit/CommitMessages.h` (`Commit`, `PackageBegin`, `PackageProgress`, `ScriptBegin`, `TransactionError`, `RpmLog`, …). See [Target/commit](08-target-commit-and-rpm.md). |
| NG media workers (`ProvideQueue` ↔ `ProvideWorker`) | worker stdin/stdout | `StompFrameStream` | `ProvideMessage` (`zyppng/lib/zypp-media/ng/private/providemessage_p.h`, codes in 7.12). See [zyppng](10-zyppng-next-generation.md). |

Test: `zypp/tests/zyppng/rpc/StompMessageStream_test.cc`.

---

## 7.14 Errors: `zypp-logic/zypp-media/mediaexception.h`

There is one class per failure mode, all derived from `MediaException`: `MediaMountException`, `MediaUnmountException`,
`MediaBadFilenameException`, `MediaNotOpenException`, `MediaFileNotFoundException`, `MediaWriteException`,
`MediaNotAttachedException`, `MediaBadAttachPointException`, `MediaSystemException`, `MediaNotAFileException`,
`MediaNotADirException`, `MediaBadUrlException`, `MediaBadUrlEmptyHostException`, `MediaUnsupportedUrlSchemeException`,
`MediaNotSupportedException`, `MediaCurlException`, `MediaCurlSetOptException`, `MediaNotDesiredException`,
`MediaIsSharedException`, `MediaNotEjectedException`, `MediaUnauthorizedException` (`hint()` = available auth types),
`MediaForbiddenException`, `MediaTimeoutException`, `MediaFileSizeExceededException`, `MediaTemporaryProblemException`,
`MediaBadCAException`, `MediaRequestCancelledException`, `MediaCurlInitException`, and more.

**Callers dispatch on `typeid(e) == typeid(X)`, not on `dynamic_cast`** (`MediaSetAccess`, `canTryNextMirror`, MediaCurl). A
subclass of e.g. `MediaFileNotFoundException` would therefore *not* be recognised as "not found". Always throw the exact class.
`FileCheckException` (`filecheckexception.h`) is used for checksum and signature failures upstream.

---

## 7.15 NG network internals in depth

Files: `zypp-logic/zypp-curl/ng/network/{networkrequestdispatcher,request,curlmultiparthandler,networkrequesterror,zckhelper}.cc`,
private state in `private/{networkrequestdispatcher_p,request_p,networkrequesterror_p,mediadebug_p}.h`. The `*_MEDIA` log macros
from `mediadebug_p.h` are only active with `ZYPP_MEDIA_CURL_DEBUG` set.

### NetworkRequestDispatcher: curl multi on the zypp event loop

```
  enqueue(req) ──► _pendingDownloads (deque, priority-ordered)
                        │ dequeuePending(): while running < _maxConnections (-1 = unlimited, default 10)
                        ▼   req->initialize() → curl_easy_init/reset + setupHandle()
                  curl_multi_add_handle, _timer->start(0)
                        │
   CURLMOPT_SOCKETFUNCTION ─► socketCallback(): one SocketNotifier per fd (Read/Write/RW), erased on CURL_POLL_REMOVE
   CURLMOPT_TIMERFUNCTION  ─► multi_timer_cb(): single-shot zyppng::Timer (stop on -1)
                        │
   notifier/timer fires ─► handleMultiSocketAction(fd, CSELECT bits) → curl_multi_socket_action
                        │   then curl_multi_info_read loop for CURLMSG_DONE:
                        │     hasMoreWork() && (OK || canRecover()) → remove, prepareToContinue(), re-add (next range batch)
                        │     else → fromCurlError() → setFinished() → sigDownloadFinished, dequeuePending()
                        ▼
   queue empty → timer stopped, sigQueueFinished
```

* Nothing is started until `run()` sets `_isRunning`. `MediaNetworkRequestExecutor` calls `run()` before each request.
* `_locked` (set while inside `curl_multi_socket_action` and `cancelAll`) blocks re-entrant `dequeuePending()`; an `OnScopeExit`
  re-runs it afterwards. A `CURLM` error cancels **all** requests and emits `sigError`.
* `setFinished()` called from inside a curl callback (e.g. the user cancels from a progress slot) only stores `_cachedResult`; the
  progress callback then returns `CURLE_ABORTED_BY_CALLBACK` and the cached result wins over the curl code.
* **There are no per-host connection limits** in the dispatcher, only the global `_maxConnections`.
  `TransferSettings::maxConcurrentConnections()` exists but the dispatcher does not read it; callers must call
  `setMaximumConcurrentConnections()` (the commit preloader does). HTTP pipelining/multiplexing is explicitly not enabled
  (commented out: "breaks our tests on releases < 15.2").
* Host-specific headers: `setHostSpecificHeader(host, name, value)` are added in `setupHandle()` for an exact `getHost()` match.
  The dispatcher's agent string overrides the `TransferSettings` user agent.
* Priorities: `Normal` (appended), `High`, `Critical=100`. `reschedule()` (via `setPriority`) stable-sorts **ascending**. See the
  Pitfalls for a bug in the insertion logic of `enqueue()`.

### NetworkRequest life cycle (`request.cc`)

The private state is a `std::variant<pending_t, running_t, prepareNextRangeBatch_t, finished_t>`:

| State | Entered by | Holds |
|---|---|---|
| `pending_t` | ctor, `reset()` in `initialize()` | the `CurlMultiPartHandler` once created |
| `running_t` | `aboutToStart()` | activity `Timer`, `AutoFILE _outFile`, byte counters, `_cachedResult`, `_isInCallback` |
| `prepareNextRangeBatch_t` | `prepareToContinue()` | the open file and helper carried over to the next batch |
| `finished_t` | `setResult()` | downloaded bytes, content length, `NetworkRequestError` |

* `setupHandle()` mirrors MediaCurl's `setupEasy()`: `FAILONERROR`, `NOSIGNAL`, `CONNECTTIMEOUT`, `TIMEOUT=3600` cap if a transfer
  timeout is set, `FOLLOWLOCATION`/`MAXREDIRS 6`, https-only redirects (`setCurlRedirProtocols`, only for `https` URLs), SSL
  options, `PROXY`/`PROXYAUTH=basic|digest|ntlm`, `PROXYUSERPWD` (falls back to `~/.curlrc`), `NOPROXY=*` for `_none_`,
  speed limits, cookie file, `PROXY_TRANSFER_MODE`. Credentials go into `CURLOPT_USERNAME`/`PASSWORD`; `CURLOPT_HTTPAUTH` is
  `authType()` or the default **`"digest,basic"`**.
* `HeadRequest` uses `NOBODY` for http(s) when `headRequestsAllowed()`, otherwise `Range: 0-1`. `ConnectionTest` uses `CONNECT_ONLY`.
* The **activity timeout** is a zyppng `Timer` started with `timeout()*1000` ms in `aboutToStart()` and restarted from the
  header/write/progress callbacks. On expiry the request is cancelled with `NetworkRequestError::Timeout`.
* Output file: opened lazily on the first body byte (`assertOutputFile()`), or on HTTP 204. `WriteExclusive` → `w+b`;
  `WriteShared` → `r+b` (fallback `w+b`), i.e. **no truncation** — MediaCurl2 relies on this so zchunk can fill an existing file.
* `setExpectedFileSize()` is enforced in `writefunction` (`ExceededMaxLen`); with ranges it is also compared against the
  `Content-Range` total ("Reported downloaded data length is not what was expected" → `InternalError`).
* `setExpectedFileChecksum()` (full-file digest): updated on the fly for plain downloads; for range downloads the file is re-read in
  `setResult()` and compared → `InvalidChecksum`. MediaCurl2 does not use it (checksums are checked upstream, chapter 8).

### Range requests and multi-part responses (`CurlMultiPartHandler`)

* Ranges are `CurlMultiPartHandler::Range` (`_start`, `_len` (0 = to EOF, only allowed as the last range), optional `Digest`,
  expected checksum bytes, `_relevantDigestLen` (compare only a prefix, e.g. SHA-512/128), `_chksumPad`, `userData`, state
  `Pending/Running/Finished/Error`). `rangedesc.h` (`zyppng::RangeDesc`) is the plain-data version of the same fields.
* `prepare()` sorts ranges, coalesces directly adjacent ones into one `a-b` spec and builds `CURLOPT_RANGE` with at most
  `_rangeAttempt[_rangeAttemptIdx]` specs; remaining ranges stay `Pending` and are fetched in the next batch on the **same
  easy handle** (`hasMoreWork()` → dispatcher re-adds it).
* Header parsing: a 2xx that is not 206, or 416, → `RangeFail` (all `Running` ranges back to `Pending`). `Content-Range:` sets the
  current server range; `Content-Type: multipart/byteranges; boundary=X` switches to multipart mode.
* Body parsing: in multipart mode each part's preface is buffered until `\r\n\r\n`, then its `Content-Range` selects the target.
  Servers may coalesce ranges, so the handler picks the requested range that starts inside the server range (exact match first,
  else smallest distance) and seeks there. Per-range digests are updated as data is written; at range end `validateRange()`
  marks it `Finished` or `Error`.
* Recovery: `canRecover()` is true only for `RangeFail` while `_rangeAttemptIdx + 1 < size-1`, so HTTP steps
  255 → 127 → 63 → 15 → 5 and then gives up with `RangeFail` (the comment says a full download is faster than 1 range per request).
  After a `RangeFail` the easy handle is `curl_easy_reset` to avoid "transfer closed with outstanding read data remaining".
  Non-HTTP (ftp) starts directly at 1 range per request.
* `verifyData()` at the end reports the first non-`Finished` range as `MissingData`, `InvalidChecksum` or `InternalError`.
  `NetworkRequest::failedRanges()` returns clones of the unfinished ranges.

### zchunk download flow (`ZckLoader` + `MediaCurl2::tryZchunk`)

Only with `ENABLE_ZCHUNK_COMPRESSION`, and only if `OnMediaLocation::deltafile()` (set via `MediaManager::setDeltafile`, i.e.
the previously cached metadata file) starts with the magic `\0ZCK1`. One `NetworkRequest` is reused for every step:

```
buildZchunkFile(target, delta, expectedSize, headerSize)
  headerSize known?  ── yes ─► [DownloadHeader] request 0..headerSize
                     └─ no ──► [DownloadLead]   request 0..zck_get_min_download_size()
cont(): DownloadLead   → zck_read_lead → request lead_len..header_len               ─► DownloadHeader
        DownloadHeader → zck_read_header; zck_find_valid_chunks; size check (≠ expected → ExceedsMaxLen);
                         ftruncate(target, zck length); zck_copy_chunks(delta → target)  (= chunk reuse)
                         request every chunk with valid==0/-1, with per-chunk digest (sha1/sha256/sha512/sha512_128)
                                                                                         ─► DownloadChunks
        DownloadChunks → zck_validate_checksums → Success (bytesReused reported)
any failure → target ftruncate(0), PrepareResult::Error
```

* `MediaCurl2` maps `Error` → **fall back to a plain full download** into the same temp file; `ExceedsMaxLen` →
  `MediaFileSizeExceededException`; `Success`/`NothingToDo` → done.
* A *transport* failure in one of the range steps is thrown as a `MediaException` straight out of `tryZchunk` (the signal handler
  calls `executeRequest`), so it does **not** fall back to a plain download on the same mirror; `getFileCopy` moves on to the next
  mirror instead. That includes a server that ignores `Range` (→ `RangeFail` → `MediaCurlException`).
* History: "Implement zchunk download, refactor Downloader backend"; "properly reset range requests (bsc#1204548)".

### Error mapping (`NetworkRequestErrorPrivate::fromCurlError`)

| curl result | NetworkRequestError | → legacy (`MediaNetworkRequestExecutor`) |
|---|---|---|
| `HTTP_RETURNED_ERROR` 401 | `Unauthorized`, or `AuthFailed` if `hasCredentials()`; `extraInfo["authHint"]` from `CURLINFO_HTTPAUTH_AVAIL` | auth callback, else `MediaUnauthorizedException` |
| 403 | `Forbidden` (SCC hint for `*.suse.com`) | `MediaForbiddenException` |
| 404, 410; `REMOTE_FILE_NOT_FOUND`, `FTP_COULDNT_RETR_FILE`, `TFTP_NOTFOUND`, **`FTP_ACCESS_DENIED`** | `NotFound` | `MediaFileNotFoundException` |
| 502, 503 | `TemporaryProblem` | `MediaTemporaryProblemException` |
| 504, `OPERATION_TIMEDOUT`, activity timer | `Timeout` | `MediaTimeoutException` |
| other HTTP codes, `PARTIAL_FILE`, unknown codes | `ServerReturnedError` | `MediaCurlException` |
| `LOGIN_DENIED` / `BAD_PASSWORD_ENTERED`, `FTP_USER_PASSWORD_INCORRECT` | `Unauthorized` / `AuthFailed` | as 401 |
| `COULDNT_RESOLVE_(PROXY|HOST)`, `COULDNT_CONNECT` | `ConnectionFailed` | `MediaCurlException` |
| `PEER_FAILED_VERIFICATION` | `PeerCertificateInvalid` | `MediaCurlException` (**not** `MediaBadCAException`) |
| `WRITE_ERROR` (also: a callback returned 0) | `InternalError` unless a cached result exists | `MediaCurlException` |
| `ABORTED_BY_CALLBACK` | `Cancelled` | `MediaRequestCancelledException` |
| `HTTP2`, `HTTP2_STREAM` | `Http2Error`, `Http2StreamError` | `MediaCurlException` |

`extraInfo` always carries `requestUrl`, `curlCode`, `filepath`, `lastRedirect`, `nativeErrorCodeDesc`/`nativeErrorDesc`.
`CURLM` errors become `InternalError` with `curlMCode`.

### How MediaCurl2 drives one file (sequence)

```
MediaCurl2::getFileCopy ─ for mirr in mirrorOrder(loc) (snapshot copy):
  mkostemp(target.new.zypp.XXXXXX) → NetworkRequest(clearQueryString(url), tmp, WriteShared)
  settings = endpoint["_zypp_transfersettings"]; setExpectedFileSize(downloadSize)
  tryZchunk() || executeRequest()
     └─ MediaCurl2::executeRequest: connect _executor->sigAuthRequired → authenticate(endpoint url, endpoint settings, ...)
          └─ MediaNetworkRequestExecutor::executeRequest: new EventLoop; dispatcher.run(); loop {
                enqueue; loop->run() until sigFinished; map error; 401 → sigAuthRequired → retry; other → maxSilentTries }
  chmod(applyUmaskTo(0644)); rename(tmp, target)
```

The executor owns **one dispatcher per handler** (created in its ctor) and a fresh `EventLoop` per call, on the thread's
`EventDispatcher` (`ThreadData::current().ensureDispatcher()`). It throws if the dispatcher still has requests after the loop.
The auth callback writes the new credentials **into the endpoint's stored `TransferSettings`**, so they stick for the rest of
the handler's life.

---

## 7.16 HTTP authentication end to end

```
  401 from server
   │ MediaCurl2/preloader: NetworkRequestError Unauthorized|AuthFailed (+authHint)   MediaCurl: evaluateCurlCode → MediaUnauthorizedException(hint)
   ▼
  MediaNetworkCommonHandler::authenticate(url, [cm,] settings, availAuthTypes, firstTry)      (static; also RepoMirrorList, CommitPackagePreloader)
   │ cm.getCred(url):  ?credentials=<file> in url → getCredFromFile();  else global set, then user set
   │ stored cred && firstTry → use it (CurlAuthData copy)
   │ else  → CurlAuthData pre-filled with username (url user if firstTry, else stored user) and authType(availAuthTypes)
   │         AuthenticationReport::prompt(url, "Authentication required for '%s'", authData) → false = cancel
   ▼
  settings.setUsername/Password; setAuthType(cred type or server hint)
  if the creds were NOT taken from the store: cred.setUrl(url); cm.addCred(); cm.save()
   ▼
  caller retries the same request (firstTry=false from now on, bsc#1210870)
```

* `AuthenticationReport` (`zypp/zypp/ZYppCallbacks.h`, `media::AuthenticationReport::prompt(Url, msg, AuthData&)`) defaults to
  `return false`, so without a UI receiver a 401 is final. zypper/YaST implement it; the `auth_data` may be a `CurlAuthData`
  carrying the available auth types.
* Auth types (`zypp-logic/zypp-curl/auth/curlauthdata.cc`): `auth_type_str2long` accepts `basic`, `digest`, `ntlm` (only if curl
  has `CURL_VERSION_NTLM`), `negotiate`/`spnego`/`gssnego` (→ `CURLAUTH_GSSNEGOTIATE`, needs SPNEGO/GSS support). Anything else
  throws `MediaException("Unsupported HTTP authentication method")`. `auth_type_long2str` emits `negotiate,ntlm,digest,basic`.
  Without an explicit type the NG request offers `digest,basic`.
* `TransferSettings::hasCredentials()` requires username **and** password (bsc#1273242); this avoids a second 401 that only
  sent the username. Together with bsc#1174011 (`auth=basic` + URL user → password preloaded from the store in
  `setupTransferSettings`).
* Credentials embedded in repo/service URLs are moved into the user store when the repo/service is added or modified:
  `UrlCredentialExtractor` (`zypp/zypp/zypp_detail/urlcredentialextractor_p.h`) used by `zypp/zypp/ng/repomanager.cc` and
  `ng/repo/workflows/serviceswf.cc`. `collect()` stores `Url` user/password via `addUserCred(url)` and saves on destruction;
  `extract()` additionally strips the password from the URL.
* **No `~/.netrc` support**: nothing in the tree sets `CURLOPT_NETRC` (grep verified). libcurl does not read `.netrc` by default.
* MediaCIFS has its own copy of the prompt logic (`MediaCIFS::authenticate`) using plain `AuthData`, triggered by the mount error
  "Permission denied".
* The NG worker path (`zypp-media-http`) has `AuthInfo`/`NoAuthData` messages instead of calling the report directly (7.12).

### Credential storage (`zypp-logic/zypp-media/auth/`)

| File | Path | Mode on save |
|---|---|---|
| global catalog | `<root>/etc/zypp/credentials.cat` (`credentials.global.file`) | 0640 |
| user catalog | `<root>/$HOME/.zypp/credentials.cat` (only if `HOME` is set; root prefix is prepended!) | 0600 |
| per-repo file | `?credentials=NAME` → `<root>/etc/zypp/credentials.d/` + **basename only** (`credentials.global.dir`); absolute path used as-is | relative: 0600, absolute: 0640 |

* `CredManagerOptions(root)` — callers pass `ZConfig::repoManagerRoot()`. The global catalog is loaded first and searched first.
* File format (`CredentialFileReader`): INI-like, multiple `[URL]` sections with the same URL but different users allowed; keys
  `username`, `password`, everything else goes to `AuthData::extraValues()`; a leading section without `[URL]` is the
  global/unnamed entry used by `credentials.d` files. Invalid URLs or entries without user+password are logged and skipped.
  Files are read and written under a `boost::interprocess::file_lock`.
* Lookup (`CredentialManager::findIn`): the request URL, rendered **without user, password and query**, is matched against the
  stored URLs, and the user must match if the request URL has one. First hit wins (set ordering is by URL string then
  username, `AuthDataComparator`). See the code for the exact matching rules.
* `addCred()` → `?credentials=` present ? `saveInFile()` (URL is blanked in the file) : `addUserCred()`. Only the password
  change of an existing (url,user) entry marks the set dirty. `save()` rewrites dirty catalogs completely.
* Saved data is plain `AuthData`: `CurlAuthData` is sliced on `addCred`, so the **auth type is not persisted**; the next session
  learns it from the server again.
* `timestampForCredDatabase(url)` (mtime of the relevant file) and `AuthData::lastDatabaseUpdate()` let callers detect that
  credentials changed on disk.
* Tests: `zypp/tests/media/CredentialManager_test.cc`, `CredentialFileReader_test.cc`.

---

## 7.17 Proxy variants and `CurlConfig` (details to 7.8)

| Class | File | Behaviour |
|---|---|---|
| `ProxyInfo` | `zypp-logic/zypp-curl/proxyinfo.cc` | Facade. Default ctor picks `ProxyInfoLibproxy` if built `WITH_LIBPROXY_SUPPORT` **and** `ProxyInfoLibproxy::isAvailabe()` (dlopen succeeds), else `ProxyInfoSysconfig("proxy")`. A new `ProxyInfo` (and file read) is created on **every** `fillSettingsSystemProxy()` call, i.e. per endpoint per attach. |
| `ProxyInfo::Impl` | `proxyinfo/proxyinfoimpl.h` | Abstract base: `enabled()`, `proxy(url)`, `noProxy*()`. Implements `useProxyFor()`: false if disabled or no proxy for the scheme; `NO_PROXY` exactly `*` disables; otherwise case-insensitive suffix match on a domain boundary, leading `.` in the pattern ignored (bnc#543337). |
| `ProxyInfoSysconfig` | `proxyinfo/proxyinfosysconfig.cc` | Reads `/etc/sysconfig/<name>` (relative names) via `sysconfig::read`. `PROXY_ENABLED != "no"` enables (default disabled if the key is missing). Per-scheme map: `HTTP_PROXY`→http, `HTTPS_PROXY`→https, `FTP_PROXY`→ftp — **no fallback** from https to `HTTP_PROXY`. `NO_PROXY` split on `, \t`. |
| `ProxyInfoLibproxy` | `proxyinfo/proxyinfolibproxy.cc` | See 7.8 (bnc#679322: force libproxy to use `/etc/sysconfig/proxy` if it exists). |
| `proxyinfos.h` | | Convenience header including both implementations. |

`fillSettingsSystemProxy()` (`curlhelper.cc`) strips user:pass out of the proxy URL into `proxyUsername/Password` (unless set
explicitly), so they reach curl as `PROXYUSERPWD`; a malformed proxy URL silently means "no proxy". `CurlConfig`
(`curlconfig.cc`) parses `$HOME/.curlrc` (only if `$HOME` is `RX` and the file `R` for the process), option names with optional
leading dashes, and keeps only `proxy-user`.

---

## 7.18 Download metadata parsers (`zypp-logic/zypp-curl/parser`)

| Class | Production users (grep verified) | Notes |
|---|---|---|
| `MetaLinkParser` | `zypp/zypp/repo/RepoMirrorList.cc` (`RepoMirrorListParseXML` → `getUrls()`) | libxml2 push parser (`parseBytes`/`parseEnd`). Metalink 3 and 4; only the first `<file>` is used. Mirror priority: `priority=` attr, else `101 - preference`, else 999999; stable-sorted ascending. Also `maxconnections`, piece hashes (`getBlockList()`), full-file sha1/sha256. `ZYPP_METALINK_DEBUG` logs the list. |
| `ZsyncParser` | only `zypp/tools/CalculateReusableBlocks.cc` | parses `.zsync` headers: URLs, block size, rsums, checksums → `MediaBlockList`. |
| `MediaBlockList` | `MetaLinkParser::getBlockList()`, tools | blocks (offset,size), per-block checksum + rsum (+pad), file checksum; `reuseBlocks(FILE*, file)` scans a local file for reusable blocks (the old multicurl delta logic). |
| `metadatahelper` (`looks_like_meta_data/file`, `minMetalinkProbeSize=256`) | **none** outside the parser dir | leftover of the MediaMultiCurl content sniffing. |

`RepoMirrorList` also accepts plain-text and JSON (`json::parseDocumentExpected`) mirror lists; format detection is by the first
character (`<` → metalink). Mirror lists are a repo topic, see [Repositories](05-repositories-and-services.md).

---

## 7.19 Mount-based handlers in depth

Common pattern in `attachTo()`: build a `MediaSource`, `findAttachedMedia()` (reuse an attach point another handler already
mounted for the same source), else `isUseableAttachPoint()`/`createAttachPoint()`, mount, `setMediaSource`, then poll `isAttached()`
up to 2-3 times with `sleep(1)` and `umount` + `MediaMountException("Unable to verify that the media was mounted")` on failure.

**`Mount`** (`zypp-logic/zypp-media/mount.cc`): runs `/bin/mount -t <fs> -o <opts> <src> <dir>` via `ExternalProgram`
(stderr→stdout, `LC_ALL=C`, optional extra environment). Output lines are mapped to `MediaMountException::mountError()` strings:
"Media already mounted", **"Permission denied"** (CIFS uses this to trigger auth), "Invalid filesystem on media", "No medium found",
"NFS path is not a directory"/"Unable to find directory on the media". `umount` retries twice after 1 s on "is busy".
`getEntries()` reads `/proc/mounts` and uses `/etc/mtab` **only if /proc/mounts yields nothing** (the code comment suggests
otherwise). Trailing `/` of the source is stripped (bnc#710269). `getMTime()` is the mtime of `/etc/mtab`, used by
`MediaHandler::checkAttached` to skip re-reading.

| Handler | Source / fs | Options | Special |
|---|---|---|---|
| NFS | `host:/path`, fs `nfs` or `nfs4` (scheme `nfs4` or `?type=nfs4`, fate#306451) | `?mountoptions=` (default `ro`). Unless both `lock`/`nolock` and `soft` are given: add `nolock` (if no lock option) and, unless `soft`/`hard` given, `soft,timeo=300` (`NFS_MOUNT_TIMEOUT`, #235211, #350309) | No credentials. |
| CIFS | `//host/share` (first path segment; `%2f` → `/`), fs always `cifs`; `_relativeRoot` = path without share | `mountoptions` + `ro` unless `rw`, `domain=` from `?workgroup`/`?domain` | Credentials: URL > `user[name]=`/`pass[word]=` in mountoptions (removed from the option string) > `CredentialManager` for the authority URL. Passed as env `USER`/`PASSWD` **and** a temp `credentials=` file (deleted right after the mount call). No creds → `guest` unless `noguest` (bnc#547354, bnc#560496). Loop: on "Permission denied" → `MediaCIFS::authenticate` prompt, save to store, retry. |
| ISO | `<isofile>` loop-mounted, fs `?filesystem=` (default `auto`) | always `ro,loop` | Parent opened in the **ctor** from `?url=` (default `dir:` + dirname of `?iso=`), preferred attach point `?mnt=`. Allowed parent schemes: `hd`, `dir`, `file`, `nfs`, `nfs4`, `smb`, `cifs` — **not http/ftp, not nested iso**. The iso file is `provideFile`d on the parent and symlinks expanded (#274651). |
| DISK (`hd:`) | `?device=` must be under `/dev/`, a block device, and found in `/dev/disk/by-uuid` or `/by-label` or accepted by `blkid -p` | `mountoptions` default `ro`, `?filesystem=` default `auto` | If the device is already mounted anywhere on the system, that mount point is **reused** (`iown=false`, never unmounted). The `bind` branch (`media->bdir`) is unreachable (bdir is never set there). |
| CD/DVD | devices from `?devices=a,b` (verified lazily as block devices) or libudev `ID_CDROM=1` (`ID_CDROM_DVD` for `dvd:`), preferring `/dev/cdrom`/`/dev/dvd` links | `mountoptions` default `ro`; tries `iso9660`, then `udf` for `dvd:` | Before mounting: reuse an existing system mount of the same major/minor (`iown=false`), else `closeTray()` then mount. `attach(next=true)` continues after `_lastdev_tried`. |

**Eject** (`zypp-logic/zypp-media/cdtools.cc`): `CDTools::openTray` = `open(O_RDONLY|O_NONBLOCK)` + `ioctl(CDROMEJECT)`, on failure
falls back to `/bin/eject <dev>`. `closeTray` uses `CDROMCLOSETRAY`. `MediaCD::releaseFrom(ejectDev)` unmounts only media it owns,
then `forceRelaseAllMedia(false)` (releases other handlers on the same device) and ejects. With `REPORT_EJECT_ERRORS 0`, eject
failures are logged only (bnc#865705: ignore failed eject on media change). `forceEject()` is used when the medium is not attached
by this handler.

Attach points (all mount handlers): `download.media_mountdir` (default `/var/adm/mount`) → `AP_0x…` dirs (7.4). There is **no
`ZYPP_*` environment variable** that affects mounting (grep verified).

---

## 7.20 `MediaPriority` and `MediaProducts`

`MediaPriority::scheme2priority` (`zypp/zypp/media/MediaPriority.cc`), `pd = ZConfig::download_media_prefer_download()`
(zypp.conf `download.media_preference`, default `download`; any value other than `volatile` means "prefer download"):

| Scheme | Priority |
|---|---|
| `file`, `dir`, `hd` | 4 |
| `nfs`, `nfs4`, `cifs`, `smb` | 3 |
| `http`, `https`, `ftp`, `tftp`, `sftp` | `pd ? 2 : 1` |
| `cd`, `dvd` | `pd ? 1 : 2` |
| anything else (`iso`, `plugin`, …) | 0 |

Higher wins. Used only as libsolv `repo->subpriority` in `PoolImpl::setRepoInfo` (computed from `RepoInfo::url()`, the first
baseurl), so it decides between packages from repos with **equal** `priority`, e.g. prefer the online repo over the DVD. The
match compares only the first character via `switch` plus the rest via `strcmp`.

`zypp/zypp/MediaProducts.h` is a **header-only, installed public API** (`productsInMedia(url, iterator|MediaProductSet&)`,
`scanProductsFile`): opens the URL with `MediaManager`, attaches, provides `media.1/products` and parses lines
`<dir> <name...>` into `MediaProductEntry{_dir,_name}`. A missing file is not an error (logged). No caller in the tree except
`zypp/tests/media/MediaProducts_test.cc`; YaST may use it (unverified). The handle is released but never `close()`d.

---

## Pitfalls & special cases

* `MediaAccess`, `MediaMultiCurl`, `MediaNetwork` and `zyppng::Downloader` are gone. Bug reports that mention them refer to old versions.
* `ZYPP_MEDIANETWORK`/`techpreview.ZYPP_MEDIANETWORK` are still *set* by ZConfig/DownloadFiles but read by nothing.
* The HTTP backend default depends on the build: Code16/TW → MediaCurl2, Code15 → MediaCurl (spec flag). `ZYPP_CURL2` overrides it, and `?mediahandler=curl|curl2` overrides both.
* There are two different `zyppng::Provide` classes: `zypp/zypp/ng/media/provide.*` (a sync facade over MediaManager, legacy) and `zyppng/lib/zypp-media/ng/provide.*` (worker-based, NG only).
* Legacy libzypp never runs `zypp-media-*` workers. They are built only with `BUILD_LIBZYPPNG`, which is off in the default spec.
* MediaCurl2 has no If-Modified-Since, no overall timeout and no metalink/zsync. Metalink is now only a repo-level mirror list source (`RepoMirrorList`).
* The default mirror order is mirrors first, then authorities. Files with `mirrorsAllowed()==false` and `doesFileExist` use authorities only.
* A failing endpoint is moved to the end of the order for the lifetime of the handler (`deprioritizeMirror`).
* Mirrors with a scheme or handler type different from the first authority are dropped silently in `MediaHandlerFactory`. Non-HTTP handlers ignore mirrors (ERR log).
* If the user changes the URL in the media change dialog, all mirrors are dropped.
* Exceptions are dispatched by exact `typeid`. Never throw a subclass when callers expect the base type.
* `MediaNetworkRequestExecutor` retries *every* error, including 404, `max_silent_tries` times. The default of 1 means no retries.
* `TransferSettings` defaults to SSL verify **off**. Only `fillSettingsFromUrl()` turns it on for https.
* Every new URL query option must also go into `clearQueryString()`, or it leaks to servers and proxies.
* Redirects are restricted to https only. A runtime curl version check decides which option to use (bsc#1218831).
* User-Agent and headers must not end in whitespace (HTTP/2 PROTOCOL_ERROR, bsc#1212187). TransferSettings trims them; keep it that way.
* `download.opensuse.org` gets the extra `X-ZYpp-AnonymousId` and `X-ZYpp-DistributionFlavor` headers (bsc#955801).
* Authentication: stored credentials may be offered only once (`firstTry`), otherwise you get endless loops (bsc#1210870). basic auth preloads the password (bsc#1174011).
* libproxy is dlopen'ed. Under YaST the *last* http proxy is used, elsewhere the first (bsc#1244710/bsc#1247690). `proxy=_none_` disables the proxy explicitly.
* `~/.curlrc` is consulted only for `proxy-user`, and only when the proxy has no credentials.
* ISO media depend on a parent handler. Release order matters (two-pass `releaseAll`, `MediaIsSharedException` on `close`).
* Mount table matching treats nfs/nfs4 and cifs/smb as equal, skips the name check for NFS, and accepts `/dev/loop*` for iso (bnc#710269, bnc#804544).
* Orphaned `AP_0x*` attach points older than a month are removed once per process. The default mountdir is `/var/adm/mount`.
* `plugin:` URLs must be resolved by a UrlResolverPlugin; `MediaPlugin` is only a stub (bsc#1228208).
* Network `provideDir/DirTree` depend on `directory.yast` on the server.
* `download.max_concurrent_connections` affects only the commit preloader. Single-file downloads are sequential.
* Dead code: `MediaCurlExceptionMayRetryInternaly` is never thrown. There is an off-by-one bounds check in `getFileUrl` (`>` instead of `>=`).
* `MediaManager.h` docs still list `mediahandler=multicurl` and mention MediaAccess. These are stale.
* **Likely bug** `NetworkRequestDispatcher::enqueue()` for non-`Normal` priority: after finding the first pending request with lower
  priority it does `it--` before `insert(it, req)`, so the new request lands *before* its predecessor. A `High` request can jump ahead
  of a pending `Critical` one, and equal priorities are LIFO.
* **Likely bug (verify)**: in the NG request the activity timer is started with `timeout()*1000` even when the timeout is 0.
  `download.transfer_timeout=0` or `?timeout=0` then probably cancel every MediaCurl2 request at once with "Download timed out",
  while MediaCurl treats 0 as "no timeout".
* zchunk with a server or proxy that ignores `Range` → `RangeFail` → `MediaCurlException` → next mirror, **no** plain-download fallback on the
  same mirror. Only `ZckLoader` errors (bad header, hash type) fall back.
* Over HTTP, range batching stops at 5 ranges per request. It never goes down to 1.
* The dispatcher has no per-host connection limit, and `TransferSettings::maxConcurrentConnections` is ignored by it.
* `MediaNetworkRequestExecutor` also counts 401s that the user did not resolve and user cancels against `max_silent_tries`.
* TLS verification failure maps to `MediaCurlException` in MediaCurl2 (not `MediaBadCAException`). `FTP_ACCESS_DENIED` maps to "not found".
* MediaCurl2's auth callback writes the new credentials into the endpoint's stored `TransferSettings`. They stay for the handler's lifetime.
* The auth type is not persisted (`CurlAuthData` is sliced to `AuthData` in `addCred`).
* The user credential path is `<root>/$HOME/.zypp/credentials.cat`. With a non-`/` root this is a path inside the target, not the real home.
* `?credentials=foo/bar` keeps only `bar` (basename) under `credentials.d`. This is deliberate and prevents path traversal.
* `~/.netrc` is never used (no `CURLOPT_NETRC`). `~/.curlrc` is used only for `proxy-user`.
* `UrlCredentialExtractor` stores `const Pathname & _root`. Constructing it from a temporary and keeping the object beyond the full expression leaves a dangling reference. Current callers are safe.
* `MediaProducts.h` defines the non-template, non-`inline` function `productsInMedia(const Url&, MediaProductSet&)` in a public header. Including it in two TUs gives a multiple-definition link error. It also never closes the access id.
* CIFS passes the password both via the `PASSWD` env and a temp credentials file. The share is the first path segment (`%2f` decoded).
* ISO parents cannot be http/ftp. The parent handler is opened in the `MediaISO` *constructor*, so a bad `url=` throws at `open()`, not at `attach()`.
* CD/DVD and `hd:` reuse an existing system mount of the same device (`iown=false`), and libzypp then never unmounts it. A desktop automount can therefore "win".
* `Mount::getEntries()` reads `/etc/mtab` only when `/proc/mounts` is empty. `checkAttached` caching relies on the mtime of `/etc/mtab`, which on modern systems is a symlink into procfs, so its mtime may not change on mount/umount (unverified effect: stale "attached" state).
* Mount errors are classified by matching English `mount` output (made safe by `LC_ALL=C`). CIFS auth only triggers on the exact "Permission denied" class.
* RPC: `PluginFrame` (blocking) and `StompFrameStream` (event-driven) are two parsers for one format. Change both.
* RPC: header values are deliberately not colon-escaped (bsc#1231043). `content-length` is always sent (bsc#1227793, zypp-plugin ≥ 0.6.4).
* **Possible stall (verify)**: `StompFrameStream::readNextMessage()` returns `false` on an empty line before a command. `readAllMessages()` then stops even though more bytes are buffered, and parsing resumes only on the next `sigReadyRead`. Our writers never send such EOLs.
* `MediaPriority` is a *solver* subpriority (equal-priority repos only), computed from the first baseurl. Its http-vs-cd order flips with `download.media_preference=volatile`.

## Where to start when debugging

* **Logs**: `/var/log/zypper.log` (or `ZYPP_LOGFILE`, `ZYPP_FULLLOG=1`). The media code logs to group `zypp::media`
  (`MediaHandler.h` sets `ZYPP_BASE_LOGGER_LOGGROUP`). Grep for `MediaHandlerFactory` "Trying scheme", "Opened:", "Attached:",
  "Trying to fetch file … via URL", "Redirecting … to geoip location", "Fetching file … from authorities only", "checkDesired".
* **Curl wire traces**: `ZYPP_MEDIA_CURL_DEBUG=2` (headers) or `3`/`4` (data). Lines are tagged by the `log_curl` function, and
  Authorization values are masked. The same variable enables `*_MEDIA` debug logs in NG `NetworkRequest`/dispatcher.
* **Isolate the backend**: run once with `ZYPP_CURL2=0` and once with `ZYPP_CURL2=1` (or use `?mediahandler=curl` on the repo URL) to see
  whether a bug is specific to MediaCurl or MediaCurl2. `ZYPP_PCK_PRELOAD=0` turns off the parallel preloader.
* **IPv6/proxy problems**: `ZYPP_MEDIA_CURL_IPRESOLVE=4`, `PX_DEBUG=1`, check `/etc/sysconfig/proxy`, `?proxy=_none_`.
* **Mirrors**: `ZYPP_METALINK_DEBUG=1` for metalink parsing. Check the mirrorlist cache and cookie in the raw cache (chapter 5).
* **NG Provide**: `ZYPP_MEDIA_PROVIDER_DEBUG=1`. Workers can be pointed at a build dir with `Provide::setWorkerPath()` (tests do this).
* **Tools**: `zypp/tools/DownloadFiles.cc` (download through the media stack with the standard progress bar),
  `zypp/tools/CalculateReusableBlocks.cc` (metalink/zsync block lists), and `zypper -vvv --debug-solver` for user-side reproduction.
* **Tests**: `zypp/tests/media/` (`MediaCurl_test.cc`, `media1..4_test.cc`, `MetaLinkParser_test.cc`, CredentialManager/Reader,
  `file_exists_test.cc`, `throw_if_not_exists_test.cc`), `zypp/tests/zypp/MediaSetAccess_test.cc`,
  `zypp/tests/zypp/MirroredOrigin_test.cc`, `zypp/tests/zyppng/media/NetworkRequestDispatcher_test.cc`, and
  `zyppng/tests/media/Provider_test.cc`. The HTTP tests use the lighttpd-based `WebServer` (`zypp-logic/tests/lib/WebServer.h`).
  Building with `-DDISABLE_MEDIABACKEND_TESTS=1` (spec `%bcond_without mediabackend_tests`) skips them (#626, #661).
* **Mount issues**: compare `MediaManager::getMountEntries()` debug dumps ("MountEntries:") with `/proc/mounts`. Look for leftover
  `AP_0x*` under `/var/adm/mount`. `Mount` logs every `mount` output line as `stdout:` at DBG, and also the final
  "mount <src> <dir>: <exit>: <err>" line. Look for "Using a shared media" and "Using a system mounted media" (reuse instead of mount).
* **Auth problems**: grep for "got stored credentials", "Found credentials for", "No credentials for", "callback answer: retry|cancel",
  "Enabling HTTP authentication methods" (NG request) and the `authHint` in the error. Check `/etc/zypp/credentials.cat`,
  `~/.zypp/credentials.cat` and `/etc/zypp/credentials.d/<name>` (file modes 0640/0600). `ZYPP_MEDIA_CURL_DEBUG=2` shows the
  `WWW-Authenticate` headers. Authorization values are masked.
* **NG request internals**: with `ZYPP_MEDIA_CURL_DEBUG` set, look for "Added easy handle", "Removing easy handle", "Requesting Ranges:",
  "Range FAIL, trying with a smaller batch", "Reached max nr of ranges", "Continuing a previously started range batch",
  "Setting activity timeout to", and "Request timeout interval". Set the variable to `>3` to log every zchunk block ("Starting block … with checksum").
  zchunk fallback is logged as "Failed to setup zchunk because of:".
* **RPC/plugins**: `ZYPP_PLUGIN_DEBUG=1` dumps plugin send/receive buffers. `ZYPP_PLUGIN_TIMEOUT`/`_SEND_TIMEOUT`/`_RECEIVE_TIMEOUT`
  adjust the 30 s default. zypp-rpm stdout/stderr appear in log group `zypp-rpm`. Parser tests are in `zypp/tests/zyppng/rpc/StompMessageStream_test.cc`.
* **More tests**: `zypp/tests/media/CredentialManager_test.cc`, `CredentialFileReader_test.cc`, `MediaProducts_test.cc`.
