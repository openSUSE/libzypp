/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_MEDIA_MEDIAURL_INCLUDED
#define ZYPP_MEDIA_MEDIAURL_INCLUDED

#include <zypp-core/Url.h>
#include <any>
#include <string>
#include <unordered_map>

namespace zypp::media {

  /*!
   * A \ref zypp::Url but extended with settings for the media backend
   */
  class ZYPP_API MediaUrl {
  public:

    using SettingsMap = std::unordered_map<std::string, std::any>;

    MediaUrl(zypp::Url url,
             std::unordered_map<std::string, std::any> settings = {});

    ~MediaUrl() = default;
    MediaUrl(const MediaUrl &) = default;
    MediaUrl(MediaUrl &&) = default;
    MediaUrl &operator=(const MediaUrl &) = default;
    MediaUrl &operator=(MediaUrl &&) = default;

    bool hasConfig( const std::string &key ) const;
    void setConfig( const std::string &key, std::any value );
    const std::any &getConfig( const std::string &key ) const;
    const SettingsMap &config() const;

    const zypp::Url &url() const;
    void setUrl(const zypp::Url &newUrl);

    template <typename T>
    std::enable_if_t<!std::is_same_v<T, std::any>> setConfig ( const std::string &key, T &&value ) {
      setConfig( key, std::make_any<T>( std::forward<T>(value) ) );
    }

    template <typename T>
    std::enable_if_t<!std::is_same_v<T, std::any>, const T&> getConfig( const std::string &key ) const {
      const std::any &c = getConfig(key);
      // use the pointer overloads to get to a const reference of the containing type
      // we need to throw std::out_of_range manually here
      const T* ref = std::any_cast<const T>(&c);
      if ( !ref )
        throw std::out_of_range("Key was not found in settings map.");

      return *ref;
    }

  private:
    zypp::Url _url;
    SettingsMap _settings;
  };

  std::ostream & operator<<( std::ostream & str, const MediaUrl & url );

  /**
   * needed for std::set
   */
  bool operator<( const MediaUrl &lhs, const MediaUrl &rhs );

  /**
   * needed for find
   */
  bool operator==( const MediaUrl &lhs, const MediaUrl &rhs );


  bool operator!=( const MediaUrl &lhs, const MediaUrl &rhs );

}


#endif
