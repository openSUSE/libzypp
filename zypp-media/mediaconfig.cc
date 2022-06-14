/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-media/mediaconfig.cc
 *
*/

#include "mediaconfig.h"
#include <zypp-core/Pathname.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/private/configoption_p.h>

namespace zypp {

  class MediaConfigPrivate {
  public:

    MediaConfigPrivate(){ }


    void set_download_retry_wait_time( long val, bool overrideDefault = false ) {
      if ( val < 0 ) val = 0;
      else if ( val > 3600 )	val = 3600;
      if ( overrideDefault ) download_retry_wait_time.setDefault( val );
      else download_retry_wait_time = val;
    }

    void set_download_max_silent_tries( long val, bool overrideDefault = false ) {
      if ( val < 0 )  val = 0;
      else if ( val > 10 ) val = 10;

      if ( overrideDefault ) download_max_silent_tries.setDefault( val );
      else  download_max_silent_tries = val;
    }

    Pathname credentials_global_dir_path;
    Pathname credentials_global_file_path;

    int download_max_concurrent_connections = 5;
    int download_min_download_speed = 0;
    int download_max_download_speed = 0;
    DefaultOption<long> download_max_silent_tries {1};
    DefaultOption<long> download_retry_wait_time  {0};
    int download_transfer_timeout = 180;
  };

  MediaConfig::MediaConfig() : d_ptr( new MediaConfigPrivate() )
  { }

  MediaConfig &MediaConfig::instance()
  {
    static MediaConfig instance;
    return instance;
  }

  bool MediaConfig::setConfigValue( const std::string &section, const std::string &entry, const std::string &value )
  {
    Z_D();
    if ( section == "main" ) {
      if ( entry == "credentials.global.dir" ) {
        d->credentials_global_dir_path = Pathname(value);
        return true;
      } else if ( entry == "credentials.global.file" ) {
        d->credentials_global_file_path = Pathname(value);
        return true;

      } else if ( entry == "download.max_concurrent_connections" ) {
        str::strtonum(value, d->download_max_concurrent_connections);
        return true;

      } else if ( entry == "download.min_download_speed" ) {
        str::strtonum(value, d->download_min_download_speed);
        return true;

      } else if ( entry == "download.max_download_speed" ) {
        str::strtonum(value, d->download_max_download_speed);
        return true;

      } else if ( entry == "download.max_silent_tries" ) {
        long val;
        str::strtonum(value, val);
        d->set_download_max_silent_tries( val, true );
        return true;

      } else if ( entry == "download.transfer_timeout" ) {
        str::strtonum(value, d->download_transfer_timeout);
        if ( d->download_transfer_timeout < 0 )		d->download_transfer_timeout = 0;
        else if ( d->download_transfer_timeout > 3600 )	d->download_transfer_timeout = 3600;
        return true;
      } else if ( entry == "download.retry_wait_time" ) {
        long val;
        str::strtonum(value, val);
        d->set_download_retry_wait_time( val, true );
        return true;
      }
    }
    return false;
  }

  Pathname MediaConfig::credentialsGlobalDir() const
  {
    Z_D();
    return ( d->credentials_global_dir_path.empty() ?
               Pathname("/etc/zypp/credentials.d") : d->credentials_global_dir_path );
  }

  Pathname MediaConfig::credentialsGlobalFile() const
  {
    Z_D();
    return ( d->credentials_global_file_path.empty() ?
               Pathname("/etc/zypp/credentials.cat") : d->credentials_global_file_path );
  }

  long MediaConfig::download_max_concurrent_connections() const
  { return d_func()->download_max_concurrent_connections; }

  long MediaConfig::download_min_download_speed() const
  { return d_func()->download_min_download_speed; }

  long MediaConfig::download_max_download_speed() const
  { return d_func()->download_max_download_speed; }

  long MediaConfig::download_max_silent_tries() const
  { return d_func()->download_max_silent_tries; }

  void MediaConfig::set_download_max_silent_tries( long val )
  {
    Z_D();
    d->set_download_max_silent_tries( val );
  }

  void MediaConfig::reset_download_max_silent_tries ()
  {
    Z_D();
    d->download_max_silent_tries.restoreToDefault();
  }

  long MediaConfig::download_retry_wait_time() const
  { return d_func()->download_retry_wait_time; }

  void MediaConfig::set_download_retry_wait_time ( long val )
  {
    Z_D();
    d->set_download_retry_wait_time( val );
  }

  void MediaConfig::reset_download_retry_wait_time ()
  {
    Z_D();
    d->download_retry_wait_time.restoreToDefault();
  }

  long MediaConfig::download_transfer_timeout() const
  { return d_func()->download_transfer_timeout; }

  ZYPP_IMPL_PRIVATE(MediaConfig)
}
