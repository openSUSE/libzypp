# Security: GPG KeyRing, Signature Verification, Checksums, Credentials & TLS

This chapter covers how libzypp decides whether data can be trusted: the two gpg keyrings
(`KeyRing`, the gpgme wrapper `KeyManagerCtx`, `PublicKey`/`PublicKeyData`), the signature
verification workflow and its user callbacks (`KeyRingReport`), the `gpgcheck`/`repo_gpgcheck`/`pkg_gpgcheck`
decision matrix, automatic key import (gpgkey URLs, `repomd.xml.key`, keyhints/buddy keys, pubkey cache),
syncing trusted keys with the rpm database (`gpg-pubkey-*`), rpm package signature checks, checksums
(`CheckSum`/`DigestReport`), and media-level security (HTTPS verification, credentials, password hiding).

Related chapters: [Repositories & services](05-repositories-and-services.md) (where the
repo download workflow runs), [Media & downloads](07-media-and-downloads.md) (curl, auth callbacks),
[Target commit & rpm](08-target-commit-and-rpm.md) (RpmDb), [Configuration](09-configuration-and-environment.md).

---

## 1. Where the code lives

The keyring code is split across tiers. Note that the *workflows* (the interactive logic)
live in `zypp/zypp/ng/workflows/` — legacy-tier code written in the "ng" async/sync pipeline style,
used by the classic API via `zyppng::Context::create()` / `defaultContext()`.

| Component | File(s) | Notes |
|---|---|---|
| Public `KeyRing`, `KeyRingReport`, `KeyRingSignals` | `zypp/zypp/KeyRing.{h,cc}` | Thin pimpl over `KeyRingImpl`; also the default-answer logic |
| `KeyRing::Impl` | `zypp/zypp/zypp_detail/keyring_p.h` | `struct KeyRing::Impl : KeyRingImpl`, wires signals to rpm callbacks |
| `KeyRingImpl`, `CachedPublicKeyData` | `zypp-logic/zypp-common/private/keyring_p.{h,cc}` | The real keyring logic, shared |
| `KeyManagerCtx` (gpgme wrapper) | `zypp-logic/zypp-common/KeyManager.{h,cc}` | All gpgme calls; log group `zypp::gpg` |
| `PublicKey`, `PublicKeyData`, `PublicSubkeyData`, `PublicKeySignatureData`, `BadKeyException` | `zypp-logic/zypp-common/PublicKey.{h,cc}` | |
| `KeyRingException` | `zypp-logic/zypp-common/KeyRingException.h` | |
| Compat forwarding headers | `zypp/zypp-compat/zypp/{KeyManager,PublicKey}.h` | Old include paths for API users |
| `KeyContext` | `zypp/zypp/KeyContext.h` | Just wraps a `RepoInfo` |
| `keyring::VerifyFileContext` | `zypp/zypp/KeyRingContexts.{h,cc}` | In/out parameter of the verify workflow |
| Verify workflow + import-from-repo | `zypp/zypp/ng/workflows/keyringwf.{h,cc}` | `zyppng::KeyRingWorkflow::verifyFileSignature`, `provideAndImportKeyFromRepository` |
| Signature file check wrapper | `zypp/zypp/ng/workflows/signaturecheckwf.{h,cc}` | Turns a "not accepted" into `SignatureCheckException` |
| Checksum workflow | `zypp/zypp/ng/workflows/checksumwf.{h,cc}` | Uses `DigestReport` |
| `FileChecker`, `ChecksumFileChecker`, `SignatureFileChecker`, `CompositeFileChecker`, `NullFileChecker` | `zypp/zypp/FileChecker.{h,cc}` | Functors used by `Fetcher`/downloaders |
| `Signature` | `zypp/zypp/Signature.{h,cc}` | Empty legacy stub class — no functionality |
| gpgkey URL download | `zypp/zypp/ng/workflows/repoinfowf.cc` (`FetchGpgKeysLogic`) | |
| Repo master index download + sig check | `zypp/zypp/ng/repo/workflows/repodownloaderwf.cc` (`DownloadMasterIndexLogic`) | |
| gpgcheck decision matrix | `zypp/zypp/RepoInfo.{h,cc}` | `repoGpgCheck()`, `pkgGpgCheck()`, `validRepoSignature()` |
| Global defaults | `zypp/zypp/ZConfig.{h,cc}`, documented in `zypp/zypp.conf.LEGACY` | `gpgcheck`, `repo_gpgcheck`, `pkg_gpgcheck`, `pubkeyCachePath()` |
| Package signature check at download | `zypp/zypp/repo/PackageProvider.cc` | `packageSigCheck`, NOKEY retry loop |
| rpm key db, sync, `checkPackage*` | `zypp/zypp/target/rpm/RpmDb.{h,cc}` | |
| `CheckSum`, `Digest` | `zypp-logic/zypp-core/{CheckSum,Digest}.{h,cc}`, `zypp/zypp/Digest.{h,cc}` (`DigestReport`), `zypp/zypp/CheckSum.h` (forward) | |
| `CredentialManager`, `CredentialFileReader`, `AuthData` | `zypp-logic/zypp-media/auth/` | compat headers in `zypp/zypp-compat/zypp/media/` |
| TLS URL options | `zypp-logic/zypp-curl/curlhelper.cc` | `ssl_verify`, `ssl_capath`, `ssl_clientcert` |
| URL password hiding | `zypp-logic/zypp-core/url/UrlBase.cc` | `ViewOption::WITH_PASSWORD`, `hotfix1050625` |
| `zypp-pubkey` tool | `zypp/tools/zypp-pubkey.cc` | Inspect rpm db keys / key files |
| Metadata verification plugins | `zypp/zypp/repo/PluginRepoverification.{h,cc}`, `zypp/zypp/repo/PluginSigcheck.{h,cc}` | §11 |
| Keyhint parsing | `zypp/zypp/parser/yum/RepomdFileReader.cc` (`keyhints()`) | §13 |

---

## 2. Concepts

### 2.1 Two keyrings, both temporary

`KeyRingImpl` (`keyring_p.h`) owns **two gpg homedirs**, both `filesystem::TmpDir`s created below the
`baseTmpDir` passed to the ctor:

| Ring | TmpDir prefix | Meaning |
|---|---|---|
| `Ring::General` (`trusted=false`) | `zypp-general-kr` | "known" keys. Anything downloaded (gpgkey URLs, `repomd.xml.key`, keyhints, cached `gpg-pubkey-*.asc`) lands here. Never sufficient to validate data on its own. |
| `Ring::Trusted` (`trusted=true`) | `zypp-trusted-kr` | Keys the user/admin trusts. Validating against this ring means "signature valid & trusted". |

The global instance is created in `zypp/zypp/zypp_detail/ZYppImpl.cc`:
`_keyring = new KeyRing(tmpPath()); _keyring->allowPreload( true );`.

**There is no persistent zypp keyring on disk.** The persistent source of truth for trusted keys is the
**rpm database** (`gpg-pubkey-<version>-<release>` pseudo packages). On target init `RpmDb` calls
`syncTrustedKeys()` (`RpmDb.cc:~335`) which exports the rpm keys into the temp trusted keyring
and vice versa (§6). The general ring is (lazily) *preloaded* from on-disk key caches (§2.4).

`enum class Ring : bool` is deliberately layout-compatible with the `bool trusted` parameter of the
public API (`using Ring = KeyRingImpl::Ring; // While "bool trusted" is in the public API`).

### 2.2 Key identity: ids, fingerprints, subkeys

`PublicKeyData` (`PublicKey.h`):

- `id()` — 16 hex-char long id of the primary key; `fingerprint()` full fpr.
- `created()` — **not** the key creation time: the timestamp of the *latest self-signature on the first uid*
  (bsc#1179222). This is what rpm uses as the release of `gpg-pubkey`, so a key whose expiry was
  extended gets a new "release". See `PublicKeyData::Impl::fromGpgmeKey`.
- `gpgPubkeyVersion()` = lowercase last 8 hex digits of id; `gpgPubkeyRelease()` = hex of `created()`
  (`date2release`); `rpmName()` = `gpg-pubkey-%v-%r`; `gpgPubkeyEdition()`.
- `providesKey(id)` matches id/fpr suffix of the primary key **or any subkey**; short 8-char ids only match the
  primary id (convenience for rpm versions). `isSafeKeyId(id)` = `id.size() >= 16`.
- `isUpdateFor(rhs)`: same fingerprint and newer `created()` (or rhs empty). Comment cites bnc#393160 (compare
  fpr, not just id) and bsc#1008325 (subkey additions do not change `created()`; not detected as update).
- `operator==` compares fingerprint + created.
- `expired()`, `daysToLive()` (INT_MAX if no expiry), `expiresAsString()` (annotates ≤90 days).

`PublicKey` = `PublicKeyData` + ASCII-armored copy of the key in a tmp file (`path()`). A file may contain
**several keys**: the API exposes the **last** key's data; others are in `hiddenKeys()`;
`fileProvidesKey(id)` also searches hidden keys. Constructing from a `Pathname` hardlink-copies the file
and parses it (`readFromFile` → `KeyManagerCtx::createForOpenPGP().readKeyFromFile`); throws
`BadKeyException` if no key found. `PublicKey::noThrow(path)` returns an empty key instead.
`isValid()` = id and fingerprint non-empty.

### 2.3 Signature key id

`KeyRingImpl::readSignatureKeyId(sig)` uses gpgme verify against `/dev/null` with a *volatile* context, collects the
fingerprints of all signatures and returns **the last one** (truncated to 16 chars, bsc#1100427 — newer gpgme
returns full fprs). The id may be a **subkey id** — that is why lookups use `providesKey`.
Throws `KeyRingException` if the signature file does not exist.

### 2.4 General ring preload (`allowPreload`)

On first access of the general ring's key list (`KeyRingImpl::publicKeyData(const Pathname&)` lazy init)
`preloadCachedKeys()` imports all files matching
`^gpg-pubkey-([[:xdigit:]]{8,})(-[[:xdigit:]]{8,})?\.(asc|key)$` from:

- `ZConfig::pubkeyCachePath()` = `repoCachePath()/"pubkeys"` (normally `/var/cache/zypp/pubkeys`)
- `/usr/lib/rpm/gnupg/keys` (distro-shipped keys)
- the same two below `systemRoot()` and `repoManagerRoot()` if not `/`.

Because import into the general ring also updates renewed trusted keys (§3.1), preload is how extended-expiry
keys get refreshed in the trusted ring/rpmdb even before they sign anything.

---

## 3. KeyRingImpl internals

### 3.1 importKey — the subtle part

`KeyRingImpl::importKey(const PublicKey&, Ring)` (`keyring_p.cc`):

```
Ring::Trusted:
  for key + each hidden key: mustUpdateData(kd, Trusted) -> Present/Missing/Update  (logged as = + >)
  if any not Present:
     gpg-import the whole file into trusted ring
     emit sigTrustedKeyAdded(key)                 (single key)
     or per key: emit sigTrustedKeyAdded(exportKey(k, Trusted))   (multi-key file:
                     re-export each key individually ascii-armored for rpm)

Ring::General:
  for key + each hidden key:
     if Present in General -> skip
     else if it is an Update of a key in Trusted -> remember in trustedToUpdate ("U" in log)
  if any not Present:
     gpg-import whole file into general ring
     SendKeys(General->Trusted) for each trustedToUpdate id   (individual keys only, never the file!)
```

bsc#1259706 (17.38.6): "Check for trusted key updates when updating the general keyring" — previously
renewed trusted keys were updated only when used to sign repo metadata.

Note the `SendKeys` path transfers keys directly via gpgme export/import into the trusted ring
(`CachedPublicKeyData::Manip`) **without emitting `sigTrustedKeyAdded`**, i.e. the rpm database is not updated
at that moment. The rpm db picks those up on the next `syncTrustedKeys(SYNC_FROM_KEYRING)` (e.g. next target
init / lock acquisition). Verify this if you touch the code: it is inferred from the code paths, not documented.

`multiKeyImport(file, ring)` is the silent raw import used by `RpmDb` (no signal, no update logic).

### 3.2 deleteKey

`deleteKey(id, ring)` looks the key up, deletes it via gpgme (`KeyManagerCtx::deleteKey` matches **primary
key id only**) and for `Ring::Trusted` emits a signal so that the rpm database is kept in sync (history:
"KeyRing: Fix deleted keys not removed from rpmdb too", bsc#1064380). The signal wiring is in
`zypp-logic/zypp-common/private/keyring_p.cc` and `zypp/zypp/KeyRing.cc`.

### 3.3 Caching key lists

`CachedPublicKeyData` caches `listKeys()` per keyring path, invalidated by `WatchFile` on
`pubring.kbx` (gpg ≥ 2.1) **and** `pubring.gpg`, plus an explicit `setDirty` whenever a `Manip` hands out a
`KeyManagerCtx` (bsc#1064380 "Flag gpg keycache dirty when adding/removing keys"; the code comment admits nobody
remembers why WatchFile alone was not enough).

### 3.4 Exporting

`exportKey(keyData, ring)` dumps the key into a `TmpFile` (`pubkey-<id>-*` in `_base_dir`) and builds a
`PublicKey` with the already-known `PublicKeyData` (no re-parse). `exportKey(id, ring)` checks existence first and
returns an empty `PublicKey` if missing (TODO comment notes the asymmetry).

---

## 4. KeyManagerCtx (gpgme wrapper)

`zypp-logic/zypp-common/KeyManager.cc`, log group `zypp::gpg`.

- `createForOpenPGP(keyring)` — gpgme ctx with `GPGME_PROTOCOL_OpenPGP`, homedir = keyring. Writes
  `gpg.conf` with `no-autostart` into the homedir if absent to **prevent launching gpg-agent** (17.38.15).
  Passing an empty `Pathname` uses gpgme defaults (i.e. the user's `~/.gnupg`!) — avoid.
- `createForOpenPGP()` — *volatile* context with its own `TmpDir` (`PublicKey.*` under `myTmpDir()`).
- `readKeyFromFile` — bsc#1140670: gpgme can't list signatures from data buffers, so the key is imported into a
  volatile keyring (wiped except `gpg.conf`) and listed from there. Non-volatile contexts delegate to a new
  volatile one.
- `listKeys` uses `GPGME_KEYLIST_MODE_LOCAL | GPGME_KEYLIST_MODE_SIGS` (sigs are needed for `created()`).
- `exportKey` uses `GPGME_EXPORT_MODE_MINIMAL` + armor (bsc#1179222: drop outdated self-sigs so rpm computes the
  right release from the first sig).
- `importKey` — bsc#1127220 workaround: gpgme may report success on interrupted import; if
  `considered == 0` but input is non-empty → treat as failure (otherwise RpmDb would report "probably v3 key").
- `verify(file, sig)` → `readSignaturesFprsOptVerify` with result evaluation (bsc#1180721):

| per-signature status | effect |
|---|---|
| no error | good |
| `GPG_ERR_KEY_EXPIRED` | **counted as good** (WAR "Accept good signature from expired key") |
| `GPG_ERR_NO_PUBKEY` | ignored (signature by a key not in this ring) |
| anything else | bad |

  Result: `!foundBad && foundGood`. So a file with multiple signatures validates if at least one is by a key in
  the ring and none is bad. **Expired keys still validate.**
- Init: `gpgme_check_version` once via `boost::call_once` (`@TODO [threading]` comment).
- `GpgmeException` derives from `KeyRingException`.

---

## 5. The verification workflow

### 5.1 Entry points

```
Fetcher / repo downloader / user code
        │  SignatureFileChecker(sig)           (FileChecker.cc)
        ▼
SignatureFileCheckWorkflow::verifySignature    (signaturecheckwf.cc)
   - sig given but missing -> SignatureCheckException("Signature … not found.")
        ▼
KeyRingWorkflow::verifyFileSignature           (keyringwf.cc, VerifyFileSignatureLogic)
   - returns pair<bool accepted, VerifyFileContext>
        ▼   accepted == false -> SignatureCheckException("Signature verification failed for …")
```

`KeyRing::verifyFileSignature()` / `verifyFileTrustedSignature()` are the *non-interactive* raw gpg checks
against one ring. The old public `KeyRing::verifyFileSignatureWorkflow` no longer exists in `KeyRing.h`; doc
comments in `FileChecker.h`/`KeyRingContexts.h` still reference it. Tests emulate it in
`zypp/tests/zypp/KeyRing_test.cc`.

`SignatureFileChecker` uses `zyppng::Context::create()` per call; `Context::keyRing()` always returns
`zypp::getZYpp()->keyRing()` (`zypp/zypp/ng/context.cc`), so it works on the global keyring.
`SignatureFileChecker::addPublicKey()` imports into the **general** ring only.
`operator()` is `const` but const_casts to store the result `VerifyFileContext` (callers read
`verifyContext().fileValidated()`).

### 5.2 VerifyFileContext

In: `file`, `signature` (may be empty), `shortFile` (default basename; used in callbacks), `keyContext`
(the `RepoInfo`), `buddyKeys` (ids to auto-import IFF validated by a trusted key).
Out: `fileAccepted` (may be true by user/default answer), `fileValidated` (signature really verified),
`signatureId`, `signatureIdTrusted` (validated by the trusted ring, not just temporarily trusted).
Stream output: `file[sig accepted:… validated:trusted|true|false(id)]`.

### 5.3 Decision tree (`VerifyFileSignatureLogic::execute` + `findKey`)

```
signature empty or missing?
   └─ yes → askUserToAcceptUnsignedFile(shortFile, keyContext)            → accepted = answer
readSignatureKeyId throws?  → not accepted
collect buddies: safe ids, not trusted, present in General, not the signing key
findKey(id):
   in Trusted?           → key, ring=Trusted, trusted
   in General?           → askUserToAcceptKey(key, ctx)
        TRUST_TEMPORARILY  → ring=General, trusted
        TRUST_AND_IMPORT   → importKey(key, trusted=true) (→ rpm import), ring=Trusted
        DONT_TRUST         → found but not trusted → NOT accepted (no further callback!)
   else, keyContext not empty → provideAndImportKeyFromRepository(id, repo)
        success → key from Trusted
   else not found
found & trusted:
   infoVerify(file, keyData, ctx)
   gpg verify against that ring
      ok   → fileValidated=true; if ring==Trusted and buddies → reportAutoImportKey(buddies, signer, ctx),
             import each buddy into Trusted;  accepted
      fail → askUserToAcceptVerificationFailed(file, key, ctx) → accepted = answer
not found:
   askUserToAcceptUnknownKey(file, id, ctx) → accepted = answer
```

`provideAndImportKeyFromRepository` (`ImportKeyFromRepoLogic`): if key known & trusted → true; if known (general)
→ ask `askUserToAcceptPackageKey` (generic `UserData` report `KeyRingReport/AcceptPackageKey`, reply bool
`"TrustKey"`) then import trusted; otherwise emit info "Repository %1% does not define 'gpgkey=' URLs." if none,
run `RepoInfoWorkflow::fetchGpgKeys` and retry from the general ring. Also used by `PackageProvider` for
`CHK_NOKEY` (§7).

### 5.4 KeyRingReport callbacks and default answers

All virtuals of `KeyRingReport` (`KeyRing.h`) return the *default answer* determined by the static
`KeyRing::DefaultAccept` flags (`KeyRing::setDefaultAccept`; default `ACCEPT_NOTHING`):

| Callback | default flag | Meaning |
|---|---|---|
| `askUserToAcceptUnsignedFile(file, ctx)` | `ACCEPT_UNSIGNED_FILE` | no/missing signature |
| `askUserToAcceptUnknownKey(file, id, ctx)` | `ACCEPT_UNKNOWNKEY` | signer key not obtainable |
| `askUserToAcceptKey(key, ctx)` → `KeyTrust` | `TRUST_KEY_TEMPORARILY` wins over `TRUST_AND_IMPORT_KEY` | key known but untrusted |
| `askUserToAcceptVerificationFailed(file, key, ctx)` | `ACCEPT_VERIFICATION_FAILED` | BAD signature |
| `infoVerify(file, keyData, ctx)` | — | informational |
| `askUserToAcceptPackageKey` (non-virtual, generic `report()`) | false unless receiver sets `"TrustKey"` | import key for rpm package check |
| `reportNonImportedKeys(set<Edition>)` | generic `KeyRingReport/KeysNotImported` | rpm keys not importable (V3, bsc#1096217) |
| `reportAutoImportKey(list, signer, ctx)` | generic `KeyRingReport/reportAutoImportKey` | buddy keys auto-imported |

The generic `UserData` reports exist because adding virtuals would break ABI. `KeyRingSignals`
(`trustedKeyAdded/Removed`) is an observer interface; `target::rpm::KeyRingSignals` is the internal channel to
RpmDb. In `KeyRing::Impl` (`KeyRing.cc`) both are sent from `sigTrustedKeyAdded/Removed`; exceptions are
caught and shown via `JobReport::error` ("hotfix for bsc#1057188").

ng wrappers for these reports: `KeyRingReportHelper`, `DigestReportHelper` in `zypp/zypp/ng/reporthelper.h`.

---

## 6. rpm database ↔ trusted keyring

`RpmDb` (`zypp/zypp/target/rpm/RpmDb.cc`):

- `KeyRingSignalReceiver` connects to `target::rpm::KeyRingSignals`: `trustedKeyAdded` → `importPubkey`,
  `trustedKeyRemoved` → `removePubkey`. Created in `RpmDb::initDatabase`-path (`sKeyRingReceiver`).
- `syncTrustedKeys(SyncTrustedKeyBits = SYNC_BOTH)` (also `importZyppKeyRingTrustedKeys()` =
  `SYNC_FROM_KEYRING`, `exportTrustedKeysInZyppKeyRing()` = `SYNC_TO_KEYRING`):
  1. `rpmKeys = pubkeyEditions()`, `zyppKeys = trustedPublicKeyData()`.
  2. If **not** `SYNC_FROM_KEYRING`: delete zypp trusted keys missing in rpm (bsc#1064380 — relieves PackageKit,
     which keeps the process alive across zypp lock re-acquisition). Uses
     `callback::TempConnect<KeyRingSignals>` to avoid echoing the delete back to rpm.
  3. `computeKeyRingSync` reduces both sets to what is missing on the other side.
  4. TO_KEYRING: write `tag_description()` of each rpm `gpg-pubkey` header into a tmp file →
     `multiKeyImport(tmp, trusted)` (signals temporarily disconnected). Keys not trusted afterwards → ERR "(V3 key?)"
     and `reportNonImportedKeys` (bsc#1096217).
  5. FROM_KEYRING: `importPubkey(exportTrustedPublicKey(k))` per key, `RpmException` swallowed.
- `importPubkey(key)`: skipped in readonly mode (bnc#828672, `zypp_readonly_hack::IGotIt()`); skipped if the same
  edition is present **unless the key has subkeys** (bsc#1008325 — subkey additions don't bump release, so always
  re-import); skipped if an older edition is requested; older editions of the same id are replaced.
- `pubkeys()` / `pubkeyEditions()` list `gpg-pubkey` packages.

### rpm package signature check

`RpmDb::checkPackage(path, detail)` (legacy, unsigned → `CHK_OK`, like `rpm -K`) vs
`RpmDb::checkPackageSignature(path, detail)` (strict, unsigned → `CHK_NOSIG`). Both go through a helper that calls
`rpmVerifySignatures` via librpm with `RPMVSF_DEFAULT`, captures rpm's log (`RpmlogCapture`) with
`LC_ALL=C` (bsc#1076415, output is parsed) and maps each indented result line:

`OK→CHK_OK, NOKEY→CHK_NOKEY, BAD→CHK_FAIL, UNKNOWN/NOTFOUND→CHK_NOTFOUND, NOTRUSTED→CHK_NOTTRUSTED, other→CHK_ERROR`.

Combination priority: FAIL > NOTFOUND > NOKEY > NOTTRUSTED > OK. If OK, bsc#1184501: a package counts as signed only
with a **header signature** and (a **payload digest** or a legacy whole-package signature). Otherwise a `CHK_NOSIG`
detail line "Package header is not signed!" / "Package payload is not signed!" is added, and with
`requireGPGSig` the result becomes `CHK_NOSIG`. For non-OK results it replaces rpm's 8-byte key ids with longer
ids from the header. `CheckPackageDetail` is a list of `(result, line)` pairs; enum strings in `operator<<` at the end
of `RpmDb.cc`.

---

## 7. gpgcheck semantics (RepoInfo / ZConfig)

Global defaults (`ZConfig.cc`): `gpgcheck = true`, `repo_gpgcheck = indeterminate`, `pkg_gpgcheck = indeterminate`,
read from `[main]` in zypp.conf (+ drop-ins). Per repo `.repo` keys `gpgcheck`, `repo_gpgcheck`, `pkg_gpgcheck`
override (`RepoInfo::Impl::_rawGpgCheck/_rawRepoGpgCheck/_rawPkgGpgCheck`, TriBool).

Effective values (`RepoInfo.cc`):

```cpp
cfgGpgCheck     = raw gpg      ?: ZConfig.gpgCheck
cfgRepoGpgCheck = (raw gpg && raw repo both unset) ? ZConfig.repoGpgCheck : raw repo   // NB: a repo-level
cfgPkgGpgCheck  = (raw gpg && raw pkg  both unset) ? ZConfig.pkgGpgCheck  : raw pkg    // gpgcheck= hides zypp.conf repo_/pkg_
repoGpgCheck()            = gpgCheck() || cfgRepoGpgCheck
repoGpgCheckIsMandatory() = (gpgCheck() && indeterminate(cfgRepoGpgCheck)) || cfgRepoGpgCheck
pkgGpgCheck()             = cfgPkgGpgCheck || (gpgCheck() && !validRepoSignature())
pkgGpgCheckIsMandatory()  = cfgPkgGpgCheck || (gpgCheck() && indeterminate(cfgPkgGpgCheck) && !validRepoSignature())
validRepoSignature()      = stored value, forced false if !repoGpgCheck()
```

Matrix from the `RepoInfo.h` doc comment:

```
 R: repo sig mandatory, confirm unsigned   r: check repo sig, unsigned ok but enforce p
 P: always check pkg sig, confirm unsigned p: like P for unsigned repos, checksum suffices for signed repos
 b: like p but accept unsigned packages
                   pkg_gpgcheck
 gpgcheck=1 |   unset    0      1
 repo_  unset/1| R/p    R/b    R/P
        0      | r/p    r/b    r/P
 gpgcheck=0 |   unset    0      1
 repo_  unset/0|               P
        1      | R      R      R/P
```

`RepoInfo::setGpgCheck(GpgCheck)` presets map to zypper options (`On`, `Strict` 111, `AllowUnsigned` 100,
`AllowUnsignedRepo` 10*, `AllowUnsignedPackage` 1*0, `Default` ***, `Off` 0**).

**Persisted signature state:** `validRepoSignature` is stored as a *symlink* `<metadataPath>/.repo_gpgcheck`
pointing to `"true"`/`"false"`/`"indeterminate"` (`internalSetValidRepoSignature`). Missing link → indeterminate
(plaindir repos, or not yet refreshed). This value decides whether packages need an rpm signature (`p` above): a
signed + validated repo secures packages via checksums in its metadata.

bsc#1274625 (17.38.15): repo_gpgcheck is now followed strictly — previously an unsigned repo accepted in the past could
stay unsigned after switching repo_gpgcheck on until the first signed metadata appeared. History of the matrix:
CVE-2017-7435/7436/9269, bsc#1045735, bsc#1038984 (16.15.0), with a gpgcheck=0 relaxation in 16.15.x only.

### 7.1 Repo metadata download & check (`DownloadMasterIndexLogic`, `repodownloaderwf.cc`)

```
provide master index (repomd.xml / content), no mirrors
fetchGpgKeys(repo)             -- every gpgkey= URL (or generated <baseurl>/<path>/repodata/repomd.xml.key,
                                  /content.key for susetags) imported into GENERAL ring; errors ignored
provide <index>.asc (optional, no mirrors)
if signed by a key still unknown → provide <index>.key (optional; via mirrors only if repo has only a
                                  mirrorlist — then warn "consider explicitly setting gpgKeyUrl")
pluginVerification()           -- repo verification plugin (exports key from keyring if .key missing)
executeSigcheckPlugins()       -- 'repo_sigcheck_plugin' extra value; mandatory sigcheck plugins (PED#11922)
signatureCheck():
   if repoGpgCheck():
      if signed || repoGpgCheckIsMandatory():
         import <index>.key into GENERAL ring (error → abort)
         keyContext = repoInfo (even if unsigned; bnc#495977)
         getExtraKeysInRepomd()  (repomd keyhints → buddy keys)
         SignatureFileCheckWorkflow::verifySignature → _repoSigValidated = fileValidated()
      else WAR "Accept unsigned repository because repoGpgCheck is not mandatory"
   else WAR "Signature checking disabled in config"
setValidRepoSignature(_repoSigValidated)   (tribool: indeterminate if never checked)
```

**Keyhints / buddy keys** (`getExtraKeysInRepomd`, only for `repomd.xml`): `RepomdFileReader::keyhints()` yields
`(file, keyid)`. Keyid must be safe (≥16 chars; bsc#1271730); file must be a plain filename (bsc#1267426,
CVE-2026-44941 — enforced in the parser, see §13). Lookup order: trusted ring → general ring →
`<repoManagerRoot>/<pubkeyCachePath>/<file>` → download `<file>` from repo media (no mirrors) and copy into the pubkey
cache (kept, `resetDispose`). Then imported into the **general** ring; ids become buddy keys. Buddies are imported
into the trusted ring **only if the repomd was validated by an already trusted key** (§5.3), announced via
`reportAutoImportKey`.

### 7.2 Package check (`PackageProvider.cc`)

If `info.pkgGpgCheck()`: `packageSigCheck` → `RpmDb::checkPackageSignature`; `CHK_NOSIG` relaxed to `CHK_OK` if not
mandatory ("Relax CHK_NOSIG"). Result is always published via `DownloadResolvableReport::pkgGpgCheck(userData)` with
keys `ResObject`, `Package` (legacy), `Localpath`, `CheckPackageResult`, `CheckPackageDetail`. On `CHK_NOKEY` the key id
is read from the rpm header and `provideAndImportKeyFromRepository` is tried; loop repeats while NOKEY and import
succeeded. Remaining errors: if the receiver set `"Action"` use it; if it set the key without value → default problem
report; else `defaultReportSignatureError`. bsc#1045735: invalid files must not remain in the package cache.

---

## 8. Checksums and DigestReport

`CheckSum` (`zypp-logic/zypp-core/CheckSum.h`): types md5, sha (alias sha1), sha1, sha224, sha256, sha384, sha512;
named ctors from string or stream. `Digest` wraps OpenSSL.

`ChecksumFileChecker` → `CheckSumWorkflow::verifyChecksum` (`checksumwf.cc`):
- empty checksum → `DigestReport::askUserToAcceptNoDigest(file)`; reject → `FileCheckException("… has no checksum")`.
- mismatch → `askUserToAcceptWrongDigest(file, requested, found)`; an acceptance is remembered **in a static map
  for up to 12h** keyed by (found → expected) to avoid re-asking when the file is re-read from cache.
- checksum computed via `Provide::checksumForFile`.
- `askUserToAccepUnknownDigest` (sic) exists in `DigestReportHelper`.

`CompositeFileChecker` runs all checkers in order; `NullFileChecker` does nothing.

---

## 9. Media-level security

### 9.1 HTTPS (`zypp-logic/zypp-curl/curlhelper.cc`)

For `https` URLs: `ssl_verify` empty or `yes` → verify peer + host; `no` → neither; otherwise comma list of
`host`,`peer` (unknown flag → `MediaBadUrlException`). `ssl_capath` must be an absolute existing dir;
`ssl_clientcert` absolute existing file (`ssl_clientkey` also supported, bnc#932393). These params plus
`proxy*`, `credentials`, `username`, `password`, `auth`, `mediahandler`, … are stripped from the URL handed to curl
(`clearQueryString`); `ssl_clientkey` is also stripped from repo URLs (bsc#1226030). `propagateQueryParams` copies
proxy and `ssl_capath`/`ssl_verify` to derived URLs (bsc#933839, boo#1127591), e.g. mirror or gpgkey URLs.

### 9.2 Credentials (`zypp-logic/zypp-media/auth/credentialmanager.cc`)

| Location | Source | Content |
|---|---|---|
| `credentials.global.dir` (default `/etc/zypp/credentials.d`) | `MediaConfig::credentialsGlobalDir()` | one file per `?credentials=<name>` URL param (relative names are basename'd into this dir) |
| `credentials.global.file` (default `/etc/zypp/credentials.cat`) | `MediaConfig::credentialsGlobalFile()` | catalog of other credentials |
| `$HOME/.zypp/credentials.cat` | `USER_CREDENTIALS_FILE` | per-user catalog |

All prefixed by the target `rootdir` (`CredManagerOptions`). Lookup `getCred(url)`: global catalog first, then user;
match = credential URL is a **prefix** of the request URL compared without username/password/query
(`findIn`), username must match if the URL has one. Files are read under a `boost::interprocess::file_lock`.
`addCred` saves to the `credentials=` file if given, else user catalog. Relevant fixes: endless loop with wrong
stored credentials (bsc#1210870), HEAD requests (bsc#1244105), `hasCredentials()` requires user **and** password
(bsc#1273242), CredentialManager export for old YaST (bsc#1228420). Details of the HTTP auth flow: see
[Media & downloads](07-media-and-downloads.md).

### 9.3 Password hiding

`Url::asString()` uses default `ViewOptions` without `WITH_PASSWORD`; `UrlBase` keeps a "safe" query string with
password-ish params (e.g. `proxypass`) hidden. `asString1050625()` (`ViewOptions::hotfix1050625`) is the hack to keep
`proxypass` when **writing .repo files** but hide it elsewhere. `ZYPP_MEDIA_CURL_DEBUG` header logging strips
credentials (bsc#1174011). Never log `asCompleteString()`.

---

## 10. zypp-pubkey tool

`zypp/tools/zypp-pubkey.cc`: `zypp-pubkey [--root DIR] [key-files...]`. Without files: dumps all `gpg-pubkey`s from the rpm
db at root (sorted by TTL). With files: prints each key's data and whether it's present in the rpm db. Handy for
checking `gpgPubkeyRelease` / expiry issues.

---

## 11. Metadata verification plugins

Two independent plugin types can veto repository metadata during `DownloadMasterIndexLogic`
(`zypp/zypp/ng/repo/workflows/repodownloaderwf.cc`). Both run **before** the gpg check (`signatureCheck()`), and
both run **regardless of `repoGpgCheck()`**. Neither influences `_repoSigValidated`: a repo accepted by a plugin but
not gpg-validated still has `validRepoSignature() == indeterminate` and needs package signatures (§7).

| | repoverification (stateless) | sigcheck (stateful, PED#11922, 17.38.8) |
|---|---|---|
| Class | `zypp_private::repo::PluginRepoverification` (`zypp/zypp/repo/PluginRepoverification.{h,cc}`) | `zypp::SigcheckPlugin`, `zypp::SigcheckPlugins` (`zypp/zypp/repo/PluginSigcheck.{h,cc}`) |
| Location | every executable file in `<pluginsPath>/repoverification` (`/usr/lib/zypp/plugins/repoverification`) | `<pluginsPath>/sigcheck/<name>` (`/usr/lib/zypp/plugins/sigcheck`; dir created by the spec) |
| Applies to | **all** repos (if at least one plugin exists) | only repos with `repo_sigcheck_plugin=` in the `.repo` file |
| Missing plugin | nothing happens | **metadata rejected** (launch fails) |
| Protocol | command-line args + exit code | `PluginScript` frames (stomp-like, see `zypp/doc/autoinclude/Plugin-Sigcheck.doc`) |
| Feature test | `libzypp(plugin:repoverification) = 0` | `libzypp(plugin:sigcheck) = 0` (`zypp/libzypp.spec.cmake`) |
| Docs | `zypp/doc/autoinclude/Plugin-Repoverification.doc` | `zypp/doc/autoinclude/Plugin-Sigcheck.doc` |
| Log group | default | `zypp::plugin` |

### 11.1 repoverification plugins

Wiring: `zyppng::RepoManager` ctor (`zypp/zypp/ng/repomanager.cc`) creates
`_pluginRepoverification( _options.pluginsPath / "repoverification", _options.rootDir )`.
`RepoManagerOptions::pluginsPath` is already `assertprefix(root, ZConfig::pluginsPath())`.
`RefreshContext` (`zypp/zypp/ng/repo/refresh.cc`) copies it only if `checkIfNeeded()`. That check uses a `WatchFile` on the
plugin dir and caches "is there at least one regular file with `userMayRX()`". `repomanagerwf.cc` passes it on to the
`DownloadContext` (`setPluginRepoverification`). In `pluginVerification()`:

1. If `<index>.asc` exists but `<index>.key` does not, read the signer id and, if the key is known in either ring,
   `dumpPublicKey` it into `<destdir>/<index>.key`. The plugin then always gets a key file when zypp knows the key.
2. `getChecker(sig, key, repoInfo)(masterIndexLocal)` → `Impl::verifyWorkflow` runs **every** executable in the dir
   (in `dirForEach` order, i.e. arbitrary) like this:

```
<plugin> --file <destdir>/repodata/repomd.xml --fsig <...>.asc --fkey <...>.key --ralias <alias>
```

   `--fsig`/`--fkey` are always passed even if the files do not exist.
   stderr is merged into stdout. A `Monitor` reads lines with an 800 ms timeout (a timeout sends a `"ping"` debug event, not an abort).
   Each line goes to a generic `UserDataJobReport{"cmdout","monitor"}` with keys `CmdId`, `CmdTag`, `CmdName`
   (`"Repoverification plugin <name>"`), `RepoInfo`. If no receiver answers the `"?"` probe with key `"!"`, the output
   is buffered and attached to the exception.
3. Exit code **≠ 0** → `PluginRepoverificationCheckException` (a `FileCheckException`):
   `"Metadata rejected by '<plugin>' plugin (returned <n>)"`. The refresh fails and the old cache is kept. Exit code 0 means accept.
   There is no "neutral" exit code.
4. **Chroot:** see `pluginVerify` for how a `rootDir` other than `/` is handled.

Typical use: site policy ("only repos signed by key X / only hosts Y"), e.g. pinning allowed keys
independently of the rpm keyring. The specific SUSE product consumers are unverified.

### 11.2 sigcheck plugins (`repo_sigcheck_plugin=`)

`.repo` syntax: `repo_sigcheck_plugin=name1 arg1 arg2;name2`. Plugins are separated by `;`, arguments by blanks, and a literal
`\`, `;` or blank is escaped with `\` (`strv::splitBSEscaped`/`unBSEscape`). `repo_sigcheck_plugin` is the only entry in
`RepoInfo::Impl::isBuiltinExtraValue` (`RepoInfo.cc`). It is kept as a raw extra value and written back to the `.repo`
file. The plugin name is turned into a path with `Pathname::assertprefix( plugindir/"sigcheck", name )`
(bsc#1265223, **CVE-2026-44933**, 17.38.9). Before that fix a name like `../../../bin/sh` or an absolute path could run any
binary. `assertprefix` handles the pathological relative roots (`.`/`..`) explicitly (`zypp-logic/zypp-core/Pathname.cc`).

Workflow (`DownloadMasterIndexLogic::execute` / `executeSigcheckPlugins`):

```
execute():  if repoInfo.hasExtraValue("repo_sigcheck_plugin"):
              SigcheckPlugins(value, ZConfig.pluginsPath()).launch(repoManagerRoot)  -- BEFORE any download
              each plugin: PluginScript::openChrooted(root)
                 -> PLUGINBEGIN {version:0}
                 <- ACK                      (no extra files)
                  | PLUGINSETUP {sig_extension:.sig, key_extension:.key}   (both optional)
                 anything else -> ProtocolError -> SigcheckPluginException("failed to launch …")
... master index, gpg keys, .asc/.key, repoverification plugins ...
executeSigcheckPlugins(): for each plugin in configured order:
     download <index><sig_extension>, <index><key_extension>   (setOptional(false), no mirrors) -> missing = error
     -> SIGCHECK {data:<path>, sig:<path>?, key:<path>?}        (paths stripprefix'ed by the chroot)
     <- ACK = ok;  ERROR/other -> SigcheckPluginException("<script> FAILED"), body (≤2048 chars) in history
signatureCheck()   (normal gpg path)
```

Timeouts are the generic `PluginScript` ones (`ZYPP_PLUGIN_TIMEOUT`, `ZYPP_PLUGIN_SEND_TIMEOUT`,
`ZYPP_PLUGIN_RECEIVE_TIMEOUT`; `ZYPP_PLUGIN_DEBUG` for frame dumps). The doc says the plugin should serve SIGCHECKs until
`_DISCONNECT`. Sending `_DISCONNECT` is up to `PluginScript::close()`/the dtor when `_sigcheckPlugins` is destroyed with
the logic object (unverified). The doc's example is a post-quantum (`pqcverification`) signature next to gpg, which is the
intended use case: extra signature schemes that gpgme can't verify. Test: `zypp/tests/repo/RepoSigcheck_test.cc`.

Note the plugin directory comes from `ZConfig::pluginsPath()` (not root-prefixed) and the plugin runs **chrooted** into
`repoManagerRoot`. This differs from repoverification, which uses the root-prefixed path and refuses to chroot.

---

## 12. End-to-end: repo metadata GPG decision flow

This expands §5.3/§7.1 into a single sequence for `repodata/repomd.xml` (susetags: `content`). `RM` = `RepoManager`
refresh (`repomanagerwf.cc`), `DL` = `DownloadMasterIndexLogic`, `VF` = `VerifyFileSignatureLogic` (`keyringwf.cc`).

```
RM refreshMetadata ─► RepoDownloaderWorkflow::download ─► DL::execute(masterIndex)
 1 launch sigcheck plugins (if repo_sigcheck_plugin)                         [fail → refresh error]
 2 provide repomd.xml (≤20MB, no mirrors)                                    [missing → error]
 3 RepoInfoWorkflow::fetchGpgKeys: for each gpgkey= URL (or generated
   <url>/<path>/repodata/repomd.xml.key | /content.key)  → KeyRing::importKey(key, false)  (GENERAL)
      └─ KeyRingImpl::importKey(General): renewed TRUSTED keys are refreshed via SendKeys (bsc#1259706)
      failures: "Key import from url … failed." (MIL) – ignored
 4 provide repomd.xml.asc (optional, no mirrors) → <destdir>
 5 if .asc present: id = readSignatureKeyId(.asc); if !isKeyKnown(id): provide repomd.xml.key
   (optional; mirrors only if repo has only a mirrorlist + warning)
 6 copy repomd.xml to <destdir>
 7 pluginVerification()        (§11.1)                                       [≠0 → error]
 8 executeSigcheckPlugins()    (§11.2)                                       [≠ACK → error]
 9 signatureCheck():
     repoGpgCheck()?  no ─► WAR "Signature checking disabled…"      → _repoSigValidated = indeterminate
     signed || repoGpgCheckIsMandatory()?  no ─► WAR "Accept unsigned repository…" → indeterminate
     ctx = VerifyFileContext(repomd.xml); ctx.signature(.asc) if signed
     .key present → importKey(PublicKey(.key), GENERAL)               [BadKeyException → error]
     ctx.keyContext(repoInfo)
     getExtraKeysInRepomd() → buddy ids (§7.1, §13)
     SignatureFileCheckWorkflow::verifySignature(ctx)
         sig given but file missing → SignatureCheckException("Signature … not found.")
         VF::execute  (decision tree §5.3, callbacks §5.4)
         !fileAccepted → SignatureCheckException("Signature verification failed for …")
     _repoSigValidated = ctx.fileValidated()        (true / false)
10 repoInfo.setMetadataPath(destdir); setValidRepoSignature(_repoSigValidated)
   → persisted as <raw cache>/.repo_gpgcheck symlink on success
```

Result states and their consequences:

| Situation | Callback (zypper prompt) | `fileAccepted` | `fileValidated` | `validRepoSignature` | Packages need rpm sig? (gpgcheck=1, pkg_ unset) |
|---|---|---|---|---|---|
| signed by key in trusted ring, good | `infoVerify` | true | true (`signatureIdTrusted`) | true | no (checksum) |
| signed by key only in general ring, user `t` | `askUserToAcceptKey`→TEMPORARILY, `infoVerify` | true | true (not trusted ring) | true | no |
| same, user `a` | `askUserToAcceptKey`→TRUST_AND_IMPORT (→ rpm `--import`) | true | true | true | no |
| same, user `r` | `askUserToAcceptKey`→DONT_TRUST | false | false | – (refresh fails) | – |
| key not obtainable | `askUserToAcceptUnknownKey` | answer | false | false | **yes** (`!bool(false)`) |
| bad signature | `askUserToAcceptVerificationFailed` | answer | false | false | yes |
| unsigned, mandatory | `askUserToAcceptUnsignedFile` | answer | false | false | yes |
| unsigned, `repo_gpgcheck=0` | none | (not run) | – | indeterminate | yes (`!bool(indeterminate)` = true) |
| `.asc` unreadable | none (logged) | false | false | – (refresh fails) | – |

`pkgGpgCheck()` uses `!bool(validRepoSignature())`. `bool(indeterminate)` is `false`, so "unsigned/unknown" and
"failed" both enforce package checks. Only a **true** stored value lets checksums secure the packages.
`validRepoSignature()` also forces a stored `true` to `false` when `repoGpgCheck()` is now off.

The original `RepoInfo.h` doc matrix, verbatim (the compact form in §7 is derived from it):

```
 R: check repo signature is mandatory, confirm unsigned repos
 r: check repo signature, unsigned repos are ok but enforce p
  : do not check repo signatures

 P: check package signature always, confirm unsigned packages
 p: like P for unsigned repos, accepted by checksum for signed repos
 b: like p but accept unsigned packages
  : do not check package signatures
                   pkg_
gpgcheck 1|     *       0       1
------------------------------------
repo_   *1|     R/p     R/b     R/P
         0|     r/p     r/b     r/P

                   pkg_
gpgcheck 0|     *       0       1
------------------------------------
repo_   *0|                       P
         1|     R       R       R/P
```

---

## 13. Keyhint hardening (bsc#1267426 / CVE-2026-44941, bsc#1271730)

**Parser side: file name only** (`zypp/zypp/parser/yum/RepomdFileReader.cc`, `RepomdFileReader::keyhints()`, 17.38.12).
Keyhints are `<keyword>`s in `repomd.xml` starting with `gpg-pubkey`:

```
gpg-pubkey-39db7c82-5847eb1f.asc?fpr=22C07BA534178CD02EFE22AAB88B2FD43DBDC284
regex:  ^(gpg-pubkey([^/?]*))(\?fpr=([[:xdigit:]]{8,}))?$
         └ keyfile (no '/' allowed)    └ keyident (if fpr= given)
else keyident from  ^-([[:xdigit:]]{8,})  applied to the remainder after "gpg-pubkey"
```

A keyword with a `/` simply does not match and is ignored (DBG only for "no keyident"). Before the fix, the keyfile part
could hold a path. It was used both as a download location on the repo media and as
`repoManagerRoot/pubkeyCachePath/<file>`, the **write target** of the cache copy in `getExtraKeysInRepomd()`, so a
malicious repo could write outside `/var/cache/zypp/pubkeys`. `..` without `/` cannot escape because the name must start
with `gpg-pubkey`.

**Workflow side: long ids only** (bsc#1271730, 17.38.15). `PublicKeyData::isSafeKeyId(id) = id.size() >= 16`
(`zypp-logic/zypp-common/PublicKey.h`) is enforced twice:

| Location | Effect |
|---|---|
| `repodownloaderwf.cc` `getExtraKeysInRepomd()` | keyhint with a short id → WAR "Keyhint is too short to safely identify a gpg key. Skip auto import:" and the hint is dropped (not even downloaded) |
| `keyringwf.cc` `VerifyFileSignatureLogic::execute()` buddy loop | buddy id too short → WAR "buddy …: key id is too short…" |
| `PublicKey.cc` `PublicKeyData::providesKey()` | short (8 char) ids match **only the primary key id**, never fingerprints/subkeys. Other short lengths never match |

Note: the regex still accepts `{8,}` hex digits, so short ids get through the parser and are rejected only in the workflow.
The changelog entry says "GPG Key hints in **repoindex.xml**", but the only keyhint code found is the repomd path above.
No service-index (`repoindex.xml`) keyhint handling was found (unverified whether the changelog wording is just imprecise).

Also part of the hardening: a downloaded keyhint file must `fileProvidesKey(keyid)` ("Keyhint … does not contain a key
with id …. Skipping it."). A cached file that doesn't provide the id is re-downloaded.

---

## 14. Package signature verification end-to-end

```
Commit / download ─► PackageProvider::providePackage (zypp/zypp/repo/PackageProvider.cc)
  cache hit (providePackageFromCache)? → returned WITHOUT a new rpm sig check (only checked files may be
              cached, bsc#1045735); toplevel-cache hits are checksum-compared then hardlinked
  download (Fetcher with checksum checker from repo metadata; policy.fileChecker = rpmSigFileChecker)
  deltarpm: rebuilt rpm is checked explicitly (rpmSigFileChecker(builddest), ~l.650) ─► rpmSigFileChecker(file)
    info.pkgGpgCheck() false ─► no rpm signature check at all (checksum only)
    loop while CHK_NOKEY:
       packageSigCheck(file, info.pkgGpgCheckIsMandatory(), userData)
           RpmDb::checkPackageSignature(file, detail)          (strict: unsigned → CHK_NOSIG)
           CHK_NOSIG && !mandatory ─► CHK_OK   ("Relax CHK_NOSIG")   ← pkg_gpgcheck=0 ("b")
           userData: CheckPackageResult, CheckPackageDetail
       report()->pkgGpgCheck(userData)                          (also on OK)
       CHK_NOKEY: id = RpmHeader::readPackage(file)->signatureKeyID()
                  KeyRingWorkflow::provideAndImportKeyFromRepository(defaultContext, id, repoInfo)
                    known+trusted  → true (loop again; rpm still lacks key? see below)
                    known general  → askUserToAcceptPackageKey ("TrustKey") → importKey(trusted) → rpm --import
                    unknown        → fetchGpgKeys(repo) then retry general ring
                  false → break
    res != CHK_OK:
       userData has value "Action"   → resolveSignatureErrorAction(action)   (IGNORE = accept insecure file)
       userData has key "Action" only → defaultReportSignatureError(res)      (problem() w/o details)
       no key                         → defaultReportSignatureError(res, detail)
       problem()/action: IGNORE → WAR "User requested to accept insecure file"; RETRY/ABORT → RpmSigCheckException
```

`DownloadResolvableReport::pkgGpgCheck(UserData)` keys (`zypp/zypp/ZYppCallbacks.h`):

| Key | Dir | Type |
|---|---|---|
| `ResObject` | in | `ResObject::constPtr` |
| `Package` | in | `Package::constPtr` (legacy) |
| `Localpath` | in | `Pathname` |
| `CheckPackageResult` | in | `RpmDb::CheckPackageResult` |
| `CheckPackageDetail` | in | `RpmDb::CheckPackageDetail` |
| `Action` | out | `DownloadResolvableReport::Action`; set the value = decision, `reset("Action")` = ask the default problem report |

**Unsigned package** (`CHK_NOSIG`): only reachable if `pkgGpgCheckIsMandatory()`. Otherwise it is relaxed to OK. Detail
lines "Package header is not signed!" / "Package payload is not signed!" come from bsc#1184501.
**Key not found** (`CHK_NOKEY`): rpm's keyring (the rpmdb `gpg-pubkey`s) lacks the key. The loop imports it into the
trusted zypp ring, which emits `sigTrustedKeyAdded` → `RpmDb::importPubkey` → `rpm --import`, and re-checks. If the key
was already in the zypp *trusted* ring but missing in rpm (e.g. the rpm import failed), `provideAndImportKeyFromRepository`
returns true without importing. The loop then re-checks and gets NOKEY again, which could spin forever. Whether
something breaks the loop in that case was not verified; watch for this in hang reports.

**At rpm install time** (`TargetImpl.cc`): the zypp-rpm single-transaction backend (`ZYPP_SINGLE_RPMTRANS`, ~l.2119)
always passes `RPMINST_NODIGEST | RPMINST_NOSIGNATURE | RPMINST_ALLOWUNTRUSTED` ("we did that already"). The classic
per-package `rpm -U` path (~l.1842) adds `--nosignature` only if `ZYppCommitPolicy::rpmNoSignature()`. Otherwise rpm verifies
again with its own `%_pkgverify_level`. A package the user accepted as unsigned/NOKEY may then still be refused by rpm when
that macro is set to `signature`/`all` (unverified on current distros).

### 14.1 zypper options → libzypp mechanism

zypper does **not** use `KeyRing::setDefaultAccept`. Its options act inside its own receivers
(`zypper/src/callbacks/keyring.h`, `repo.h`; flags in `zypper/src/Config.cc`):

| zypper option | Effect in libzypp terms |
|---|---|
| `--no-gpg-checks` | `askUserToAcceptUnsignedFile`/`UnknownKey`/`VerificationFailed` → `true` (warning printed); `askUserToAcceptKey` → `KEY_TRUST_TEMPORARILY` (only where temporary trust is allowed); `pkgGpgCheck` → sets `"Action" = IGNORE` for every non-OK result; Digest `askUserToAcceptNoDigest`/`UnknownDigest` default to yes. **Does not change** `repoGpgCheck()`/`pkgGpgCheck()`. Checks still run, only the answers change, so `fileValidated` stays false and `validRepoSignature=false` |
| `--gpg-auto-import-keys` | `askUserToAcceptKey` → `KEY_TRUST_AND_IMPORT` (→ rpm import). No effect on unsigned/bad files |
| `--gpgcheck`, `--gpgcheck-strict`, `--gpgcheck-allow-unsigned[-repo|-package]`, `--no-gpgcheck` (addrepo/modifyrepo) | `RepoInfo::setGpgCheck(GpgCheck::…)` → writes the `.repo` tri-states (§7) |
| `--insecure` | no such option was found in `zypper/src` (only a V3-key message mentions "insecure") |

Non-root without `--root`: zypper's `askUserToAcceptKey` drops the `a` (import) choice. If temporary trust is not
allowed either, it returns `KEY_DONT_TRUST`.

---

## 15. Key lifecycle

| Event | Code path | rpmdb effect |
|---|---|---|
| Process start | `ZYppImpl` creates `KeyRing(tmpPath())`, both rings empty | – |
| Target init / lock | `RpmDb::syncTrustedKeys(SYNC_BOTH)` (§6) | rpm `gpg-pubkey`s → trusted ring; trusted-only keys → `rpm --import` |
| First general-ring access | `preloadCachedKeys()` from pubkey cache + `/usr/lib/rpm/gnupg/keys` (§2.4) | none directly (renewed trusted keys via `SendKeys`, picked up at next sync) |
| Repo refresh | `fetchGpgKeys` → general ring (every refresh, every gpgkey URL) | same as above: a renewed key is refreshed this way. This is how "zypper ref" refreshes expiring keys |
| User trusts key (`a`) | `importKey(key, true)` → `sigTrustedKeyAdded` → `RpmDb::importPubkey` | `rpm -e --allmatches gpg-pubkey-<ver>` if an older release exists, then `rpm --import -- <armored file>` |
| Buddy key auto-import | VF after a trusted validation → `importKey(buddy, true)` + `reportAutoImportKey` | `rpm --import` |
| Package NOKEY | §14 | `rpm --import` |
| `KeyRing::deleteKey(id, true)` | `KeyRingImpl::deleteKey` → trusted-ring signal (§3.2) | rpmdb sync |
| `RpmDb::removePubkey(key)` | direct API | `rpm -e -- gpg-pubkey-<ver>-<rel>`, removing only the **first** edition found with that version |
| rpm keys deleted outside zypp | next `syncTrustedKeys` without `SYNC_FROM_KEYRING` deletes them from the zypp trusted ring (bsc#1064380) | – |

`gpg-pubkey` naming: `version` = last 8 hex digits of the key id (lowercase), `release` = hex of `PublicKeyData::created()`
(newest self-signature, bsc#1179222). An extended key therefore becomes a *new release* of the same version, and
`importPubkey` replaces all older releases. Equal edition with subkeys → re-import anyway (bsc#1008325).

**Expired keys:** libzypp has no `reportKeyExpired` callback and no proactive expiry warning. `PublicKeyData::expired()`,
`daysToLive()` and `expiresAsString()` (marks ≤90 days) exist for front ends, e.g. zypper's key dump and `zypp-pubkey`.
gpg verification still **accepts** signatures from expired keys (§4). The only handling is renewal: a newer copy arriving
through gpgkey URLs, `.key`, the keyhint cache or `/usr/lib/rpm/gnupg/keys` updates the trusted ring (bsc#1259706).

**rpm ≥ 4.20 / rpm-sequoia:** the code has no special handling (no `sequoia`/`rpmkeys` references in `zypp/zypp`).
Key import/removal shells out to `rpm --import` / `rpm -e gpg-pubkey-…` (`run_rpm`), and listing iterates rpmdb headers
named `gpg-pubkey` (`librpmDb::db_const_iterator::findByName`). With an rpm configured for a non-rpmdb keystore
(`%_keyring fs`/`openpgp`), `pubkeys()` would see nothing and `rpm -e gpg-pubkey…` would fail. That would break
`syncTrustedKeys` and make rpm and zypp diverge (unverified, check before enabling such a keystore). The V3-key
detection (`reportNonImportedKeys`, bsc#1096217) relies on gpgme being unable to import an rpm key. Sequoia rejects more
legacy keys, so expect more of these reports.

---

## 16. Security-relevant configuration

| Setting | Where | Default | Effect |
|---|---|---|---|
| `gpgcheck` | zypp.conf `[main]`, `.repo` | `on` | master switch (§7) |
| `repo_gpgcheck` | zypp.conf, `.repo` | unset → follows gpgcheck | metadata check / mandatory-ness |
| `pkg_gpgcheck` | zypp.conf, `.repo` | unset → follows gpgcheck | rpm sig check / mandatory-ness |
| `gpgkey=` | `.repo` | – (generated `repomd.xml.key`/`content.key`) | keys imported into the general ring each refresh |
| `repo_sigcheck_plugin=` | `.repo` | – | mandatory sigcheck plugins (§11.2) |
| `path=` | `.repo` | `/` | `RepoInfo::setPath` stores `absolutename()`, so it can't climb above the baseurl (bsc#1267874, CVE-2026-44942) |
| `credentials.global.dir` / `.file` | zypp.conf | `/etc/zypp/credentials.d`, `/etc/zypp/credentials.cat` | §9.2 |
| `ssl_verify`, `ssl_capath`, `ssl_clientcert`, `ssl_clientkey` | URL query | verify on | §9.1 |
| `[env]` section | zypp.conf (17.38.10) | – | sets env vars for the process (debug knobs below can thus be set persistently) |
| `ZYPP_CONF` | env | `/etc/zypp/zypp.conf` | alternative config, so all of the above can be overridden |
| `ZYPPTMPDIR` | env | `/var/tmp` | base of `TmpPath`. Where the temp gpg homedirs live |
| `ZYPP_READONLY_HACK` | env | – | readonly mode, no rpm key import (bnc#828672) |
| `ZYPP_LOCKFILE_ROOT` | env | `/` | location of the zypp lock |
| `ZYPP_SINGLE_RPMTRANS` | env | build-dependent | zypp-rpm backend: rpm runs with `--nosignature --nodigest` (§14) |
| `ZYPP_PLUGIN_TIMEOUT`, `_SEND_TIMEOUT`, `_RECEIVE_TIMEOUT`, `ZYPP_PLUGIN_DEBUG` | env | – | sigcheck plugin I/O (§11.2) |
| `ZYPP_MEDIA_CURL_DEBUG` | env | – | header logging (credentials stripped, bsc#1174011) |
| `ZYPP_RPM_DEBUG` | env | – | rpm verbosity (key import output) |
| `HOME` | env | – | `~/.zypp/credentials.cat`, curl config |
| `ZYppCommitPolicy::rpmNoSignature()` | API | false | `--nosignature` in the classic commit path |
| `KeyRing::setDefaultAccept()` | API | `ACCEPT_NOTHING` | default answers when no receiver is connected (§5.4) |

---

## 17. CVE history (from `zypp/package/libzypp.changes`)

| CVE | bsc | Version | Description | Code area |
|---|---|---|---|---|
| CVE-2017-7435, CVE-2017-7436, CVE-2017-9269 | bsc#1045735, bsc#1038984 | 16.15.0 | Unsigned repos/packages accepted without proper checks; added `checkPackageSignature`, mandatory pkg sig, gpgcheck matrix | `RepoInfo.cc`, `PackageProvider.cc`, `RpmDb.cc`, repo downloader |
| CVE-2017-9269 | bsc#1045735 | 16.15.1 | Bad packages must not stay in the package cache | `PackageProvider.cc` |
| CVE-2018-7685 | bsc#1091624, bsc#1088705 | 17.5.0 | Validate delta rpms before caching | `PackageProvider.cc` (`RpmPackageProvider`, deltarpm) |
| CVE-2019-18900 | bsc#1158763 | 17.19.0 | Cookie file must have mode 0600 | `MediaCurl` cookie file (now `zypp-curl`/`MediaCurl.cc`, exact place unverified) |
| CVE-2017-9271 | bsc#1050625, bsc#1177583 | 17.25.3 | Hide password entries in URL query when writing/logging | `UrlBase.cc` (`hotfix1050625`, §9.3) |
| CVE-2026-25707 | bsc#1259802 | 17.38.10 | Metadata entries referring to locations outside the repo are discarded | metadata parsers / file collectors (exact place unverified) |
| CVE-2026-44933 | bsc#1265223 | 17.38.9 | `repo_sigcheck_plugin` name could escape the sigcheck dir | `PluginSigcheck.cc` (`assertprefix`) |
| CVE-2026-44941 | bsc#1267426 | 17.38.12 | Keyhint must be a file name, not a path | `RepomdFileReader::keyhints()` |
| CVE-2026-44942 | bsc#1267874 | 17.38.13 | `.repo` `path=` must not point outside the repo | `RepoInfo::setPath` (`absolutename()`) |

Related security fixes without a CVE in the changelog: bsc#1271730 (keyhint long ids, 17.38.15), bsc#1274625 (strict
repo_gpgcheck, 17.38.15), bsc#1259706 (trusted key renewal, 17.38.6), gpg-agent no-autostart (17.38.15).

---

## Pitfalls & special cases

- There is no persistent zypp keyring: both rings are tmp dirs; the rpm db is the persistent trusted store; the general ring is rebuilt from `/var/cache/zypp/pubkeys` + `/usr/lib/rpm/gnupg/keys` on first use.
- `PublicKeyData::created()` is the newest self-signature time, not the key creation time (bsc#1179222); it determines the `gpg-pubkey` release. Changing it breaks rpm db sync.
- Subkey changes do not bump `created()` (bsc#1008325): `isUpdateFor` misses them, so `RpmDb::importPubkey` always re-imports keys that have subkeys.
- Signatures may be made by subkeys; always look keys up with `providesKey`, never by comparing ids. `KeyManagerCtx::deleteKey`/`findKeyById` match only the primary key id.
- Signatures from **expired keys still validate** (`GPG_ERR_KEY_EXPIRED` is treated as good); unknown-key signatures are ignored as long as one good one exists (bsc#1180721).
- `readSignatureKeyId` returns only the *last* signature's id when there are several.
- A key file can hold several keys; `PublicKey` shows the last one, the rest are in `hiddenKeys()`. Importing imports all of them.
- Renewed trusted keys are updated from the general ring through `SendKeys` without a rpm signal (bsc#1259706); rpm only gets them on the next sync.
- `KEY_DONT_TRUST` for a known key ends verification as "not accepted" with no further callback; `TRUST_KEY_TEMPORARILY` validates but sets `signatureIdTrusted=false`, so buddy keys are **not** imported.
- `fileAccepted` ≠ `fileValidated`: default answers / user input can accept unsigned or bad files. Only `fileValidated` is stored as `validRepoSignature`.
- `validRepoSignature` is stored as the `.repo_gpgcheck` symlink in the metadata dir and decides whether packages need rpm signatures. Wiping or copying caches changes package checking.
- A repo-level `gpgcheck=` hides the zypp.conf `repo_gpgcheck`/`pkg_gpgcheck` defaults for that repo (see `cfgRepoGpgCheck`).
- repo_gpgcheck is strict as of bsc#1274625: switching it on means the next metadata must be signed.
- Keyhints and buddy ids must be long ids or fingerprints (bsc#1271730); keyhint file names must be plain names (bsc#1267426/CVE-2026-44941). They only go into the trusted ring if repomd was validated by an already trusted key.
- `.key` files next to repomd are fetched from mirrors only if the repo has nothing but a mirrorlist (prints a warning).
- `KeyManagerCtx::createForOpenPGP(Pathname())` uses the caller's `~/.gnupg`. Every keyring homedir gets a `gpg.conf` with `no-autostart`.
- gpgme import may report success after a signal (bsc#1127220); the "considered==0" check catches this. Keep it.
- `CheckPackageResult` parsing depends on rpm's text output in the C locale (bsc#1076415). Header sig + payload digest are required to count as signed (bsc#1184501).
- Wrong-checksum acceptances are cached in a process-wide static map for 12h (`checksumwf.cc`).
- `KeyRingReport` virtuals cannot be extended (ABI). New interactions use generic `UserData` reports (`AcceptPackageKey`, `KeysNotImported`, `reportAutoImportKey`).
- In readonly mode no keys are imported into rpm (bnc#828672).
- Plugins (both kinds) run before and independently of `repoGpgCheck()` and never set `validRepoSignature`. A plugin-verified but gpg-unsigned repo still enforces rpm package signatures.
- Sigcheck plugins are launched before the master index is downloaded, and their sig/key files are **non-optional**: a missing `repomd.xml<sig_extension>` makes the refresh fail. A configured but missing plugin also rejects the repo.
- `repo_sigcheck_plugin` names are confined to `plugins/sigcheck` via `Pathname::assertprefix` (CVE-2026-44933). Never build plugin paths with plain `operator/`.
- `--no-gpg-checks` changes only zypper's callback answers. libzypp still runs every check, `fileValidated` stays false, so `validRepoSignature=false` and package checks stay enforced (and get auto-ignored by zypper).
- zypp-rpm (single transaction) installs with `--nosignature --nodigest` + allow-untrusted, because libzypp has already checked the packages. The classic `rpm -U` path lets rpm re-verify unless `rpmNoSignature()` is set.
- `RpmDb::removePubkey` removes only the first `gpg-pubkey-<ver>-*` release found, while `importPubkey` removes all of them (`--allmatches`).
- There is no expiry callback. Expired keys keep validating, and renewal relies on gpgkey URLs/caches refreshing the trusted ring.
- No rpm-sequoia / non-rpmdb keystore support: key sync assumes `gpg-pubkey` headers in the rpmdb and the `rpm --import`/`rpm -e` CLI.

## Where to start when debugging

- **Logs** (`/var/log/zypper.log`, or `ZYPP_LOGFILE=-`/`ZYPP_FULLLOG=1`, see [Configuration](09-configuration-and-environment.md)): grep the log groups `zypp::KeyRing` (import fate lines `Import Tkey`/`Import Gkey` with `=`/`+`/`>`/`U` markers, "Going to verify signature", `askUserTo…:` answers, "buddy …"), `zypp::gpg` (gpgme errors, "Accept good signature from expired key", "Failed signature check"), `FileChecker`, and RpmDb lines "Going to sync trusted keys…", "Rpm keys to export…", "Could not import key … (V3 key?)".
- `VerifyFileContext` stream output in the log shows `accepted:` / `validated:trusted|true|false(id)` in one line.
- `RepoInfo` dump (`dumpOn`) shows `gpgcheck : … repo… sig? pkg…` with `*` for mandatory — the quickest way to check the decision matrix for a repo. Look at the `.repo_gpgcheck` symlink in `/var/cache/zypp/raw/<alias>/`.
- Inspect keys: `zypp-pubkey` (rpm db and key files), `rpm -q gpg-pubkey --qf '%{NAME}-%{VERSION}-%{RELEASE} %{SUMMARY}\n'`, `rpmkeys -Kv pkg.rpm` (same lines `checkPackageSignature` parses), `gpg --homedir <tmp keyring> --list-keys` (rings are under ZYpp's tmp path while the process runs).
- Pubkey cache: `/var/cache/zypp/pubkeys/` (keyhint downloads, `gpg-pubkey-*.asc` preload); distro keys `/usr/lib/rpm/gnupg/keys/`.
- Tests: `zypp/tests/zypp/KeyRing_test.cc` (workflow with `KeyRingTestReceiver.h`, data in `tests/zypp/data/KeyRing`), `PublicKey_test.cc`, `FileChecker_test.cc`, `Signature_test.cc`, `CheckSum_test.cc`, `Digest_test.cc`, `tests/media/CredentialManager_test.cc`, `CredentialFileReader_test.cc`. See [Testing](11-testing.md).
- The `GpgCheck` enum comments name the zypper options (`--gpgcheck-strict`, `--no-gpgcheck`, …). zypper's `--no-gpg-checks` / `--gpg-auto-import-keys` are implemented in its own receivers (`zypper/src/callbacks/keyring.h`, `repo.h`), not via `KeyRing::setDefaultAccept` (§14.1). Look there first when you get "silently accepted" reports.
- Plugin rejects: look for "Metadata rejected by '<plugin>' plugin (returned N)" (repoverification) or "<script> FAILED" / "protocol error: got … expected …" (sigcheck, log group `zypp::plugin`; set `ZYPP_PLUGIN_DEBUG=1` for frame dumps). Run a repoverification plugin by hand with `--file/--fsig/--fkey/--ralias` against `/var/cache/zypp/raw/<alias>/repodata/`.
- Keyhint problems: grep "Check keyhints", "Keyhint is too short", "Keyhint … does not contain a key", "buddy …" in the log. Inspect `<keyword>gpg-pubkey…</keyword>` in `repomd.xml`.
- Package signature failures: the `pkgGpgCheck` report data (zypper prints `CheckPackageDetail`), "Relax CHK_NOSIG", "User requested to accept insecure file". Reproduce with `rpmkeys -Kv`.
- Key lifecycle: RpmDb log lines "Key … will be imported into the rpm trusted keyring.(update|new)", "… is older than one in the rpm trusted keyring", "Failed to remove key …".
- Credentials: `ZYPP_MEDIA_CURL_DEBUG=1` logs HTTP headers with credentials stripped. Check `/etc/zypp/credentials.d/<name>` permissions (`userMayR` is required) and prefix matching of the stored URL.
