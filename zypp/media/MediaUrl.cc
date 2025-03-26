/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaUrl.cc
*/
#include "MediaUrl.h"

namespace zypp::media {

  MediaUrl::MediaUrl(Url url, std::unordered_map<std::string, std::any> settings)
    : _url( std::move(url) )
    , _settings( std::move(settings) )
  { }

  bool MediaUrl::hasConfig(const std::string &key) const
  {
    return (_settings.count (key) > 0);
  }

  void MediaUrl::setConfig(const std::string &key, std::any value)
  {
    _settings.insert_or_assign ( key, std::move(value) );
  }

  const std::any &MediaUrl::getConfig(const std::string &key) const
  {
    return _settings.at(key);
  }

  const MediaUrl::SettingsMap &MediaUrl::config() const
  {
    return _settings;
  }

  const zypp::Url &MediaUrl::url() const
  {
    return _url;
  }

  void MediaUrl::setUrl(const zypp::Url &newUrl)
  {
    _url = newUrl;
  }

  std::ostream & operator<<( std::ostream & str, const MediaUrl & url )
  {
    return str << url.url().asString();
  }

  bool operator<( const MediaUrl &lhs, const MediaUrl &rhs )
  {
    return (lhs.url().asCompleteString() < rhs.url().asCompleteString());
  }

  bool operator==( const MediaUrl &lhs, const MediaUrl &rhs )
  {
    return (lhs.url().asCompleteString() == rhs.url().asCompleteString());
  }

  bool operator!=( const MediaUrl &lhs, const MediaUrl &rhs )
  {
    return (lhs.url().asCompleteString() != rhs.url().asCompleteString());
  }


}
