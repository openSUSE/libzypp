/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-media/ng/config/mediakeys.h
 *
 * Config key tokens owned by the media tier. They only use core value types
 * (long, bool, zypp::Pathname, std::set<std::string>) so media workers can
 * register them without linking application-layer types (Arch, Locale, ...).
 *
 * A token is defined once; its *scope* is decided by which Config instance a
 * consumer is handed (see the "Scope:" comment on each token).
 */
#ifndef ZYPP_MEDIA_NG_CONFIG_MEDIAKEYS_H
#define ZYPP_MEDIA_NG_CONFIG_MEDIAKEYS_H

#include <algorithm>
#include <set>
#include <string>

#include <zypp-core/Pathname.h>
#include <zypp-core/ng/config/config.h>
#include <zypp-core/ng/config/configparsers.h>

namespace zyppng::config {

  // ── Credentials ───────────────────────────────────────────────────────────
  // Scope: CONTEXT — resolve via the Context that owns the repo (see media-config.md Phase 5).
  inline constexpr Key<zypp::Pathname> CredentialsDir {
    "media/credentials.dir", "credentials.global.dir",
    [](const Config&) { return zypp::Pathname("/etc/zypp/credentials.d"); },
    parsePath, serializePath };
  // Scope: CONTEXT — resolve via the Context that owns the repo (see media-config.md Phase 5).
  inline constexpr Key<zypp::Pathname> CredentialsFile {
    "media/credentials.file", "credentials.global.file",
    [](const Config&) { return zypp::Pathname("/etc/zypp/credentials.cat"); },
    parsePath, serializePath };

  // ── Geo IP behaviour ──────────────────────────────────────────────────────
  // Scope: HOST
  inline constexpr Key<bool> GeoipEnabled {
    "media/geoip.enabled", "download.use_geoip_mirror",
    [](const Config&) { return true; },
    parseBool, serializeBool };

  // Scope: HOST
  inline constexpr Key<std::set<std::string>> GeoipHostnames {
    "main/geoip.hosts", "",
    [](const Config&) {
      std::set<std::string> hosts;
      hosts.insert("download.opensuse.org");
      return hosts;
    },
    parseStringSet, serializeStringSet };

  // ── Download transport (mapped to "media/" section) ───────────────────────
  // Scope: HOST — consumed by media workers via the host Config.
  inline constexpr Key<long> MaxConcurrentConnections {
    "media/max_concurrent_connections", "download.max_concurrent_connections",
    [](const Config&) { return 5L; },
    parseLong, serializeLong };

  // Scope: HOST — consumed by media workers via the host Config.
  inline constexpr Key<long> MinDownloadSpeed {
    "media/min_download_speed", "download.min_download_speed",
    [](const Config&) { return 0L; },
    parseLong, serializeLong };

  // Scope: HOST — consumed by media workers via the host Config.
  inline constexpr Key<long> MaxDownloadSpeed {
    "media/max_download_speed", "download.max_download_speed",
    [](const Config&) { return 0L; },
    parseLong, serializeLong };

  // Scope: HOST — consumed by media workers via the host Config.
  inline constexpr Key<long> MaxSilentTries {
    "media/max_silent_tries", "download.max_silent_tries",
    [](const Config&) { return 1L; },
    parseLong, serializeLong };

  // Scope: HOST — consumed by media workers via the host Config.
  inline constexpr Key<long> TransferTimeout {
    "media/transfer_timeout", "download.transfer_timeout",
    [](const Config&) { return 180L; },
    // clamped [0, 3600] — matches legacy ZConfig/MediaConfig behaviour
    [](std::string_view s) -> expected<long> {
      const auto& expV = parseLong(s);
      if (!expV) return expV;
      long v = *expV;
      if (v < 0)    v = 0;
      if (v > 3600) v = 3600;
      return v;
    },
    serializeLong };

  // Scope: HOST — consumed by media workers via the host Config.
  inline constexpr Key<long> ConnectTimeout {
    "media/connect_timeout", "download.connect_timeout",
    [](const Config&) { return 60L; },
    // clamped [0, ∞) — negative values silently become 0
    [](std::string_view s) -> expected<long> {
      const auto& expV = parseLong(s);
      if (!expV) return expV;
      return std::max(0L, *expV);
    },
    serializeLong };

  // Scope: HOST
  // Host-relative (not root-relative) — the download machinery mounts
  // physical media on the running host, not inside the managed root.
  inline constexpr Key<zypp::Pathname> MediaMountDir {
    "media/media_mountdir", "download.media_mountdir",
    [](const Config&) { return zypp::Pathname("/var/adm/mount"); },
    [](std::string_view s) -> expected<zypp::Pathname> {
      return zypp::Pathname( std::string(s) );
    },
    serializePath };

  /** Register all media-tier keys. Called by registerZyppKeys() and by media workers. */
  void registerMediaKeys( Config & cfg );

} // namespace zyppng::config

#endif // ZYPP_MEDIA_NG_CONFIG_MEDIAKEYS_H
