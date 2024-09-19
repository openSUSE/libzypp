/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_MEDIA_NG_LAZYMEDIAHANDLE_H
#define ZYPP_MEDIA_NG_LAZYMEDIAHANDLE_H

#include <zypp-media/ng/ProvideSpec>
#include <optional>


namespace zyppng
{
  ZYPP_FWD_DECL_TYPE_WITH_REFS (MediaContext);

  template < class ProvideType >
  class LazyMediaHandle {
  public:
    using MediaHandle = typename ProvideType::MediaHandle;
    using ParentType  = ProvideType;

    friend ProvideType;

    LazyMediaHandle( Ref<ProvideType> provider, std::vector<zypp::Url> urls, ProvideMediaSpec spec )
      : _sharedData(
          std::make_shared<Data>(
            std::move(provider)
            ,std::move(urls)
            ,std::move(spec)
            ))
    { }

    LazyMediaHandle(const LazyMediaHandle &) = default;
    LazyMediaHandle(LazyMediaHandle &&) = default;
    LazyMediaHandle &operator=(const LazyMediaHandle &) = default;
    LazyMediaHandle &operator=(LazyMediaHandle &&) = default;

    const Ref<ProvideType> &parent () const {
      return _sharedData->_provider.lock();
    }

    /*!
     * Returns the first mirror used for this lazy handle,
     * otherwise returns a empty url.
     */
    const zypp::Url &baseUrl() const {
      if ( !_sharedData->_mediaHandle ) {
        static zypp::Url invalidHandle;
        if ( _sharedData->_urls.empty() )
          return invalidHandle;
        return _sharedData->_urls.front();
      } else {
        return _sharedData->_mediaHandle->baseUrl();
      }
    }

    const std::vector<zypp::Url> &urls() const {
      return _sharedData->_urls;
    }

    std::optional<MediaHandle> handle () const {
      return _sharedData->_mediaHandle;
    }

    const ProvideMediaSpec &spec() const {
      return _sharedData->_spec;
    }

    const std::optional<zypp::Pathname> &localPath() const
    {
      static std::optional<zypp::Pathname> noHandle;
      if ( !attached() )
        return noHandle;
      return _sharedData->_mediaHandle.localPath();
    }

    bool attached() const {
      return _sharedData->_mediaHandle.has_value();
    }

  private:

    struct Data {
      Data(Ref<ProvideType> &&provider, std::vector<zypp::Url> &&urls,
           ProvideMediaSpec &&spec)
        : _provider(std::move(provider)), _urls(std::move(urls)),
          _spec(std::move(spec)) {}

      Data(const Data &) = delete;
      Data(Data &&) = delete;
      Data &operator=(const Data &) = delete;
      Data &operator=(Data &&) = delete;

      WeakRef<ProvideType>       _provider;
      std::vector<zypp::Url>     _urls;
      ProvideMediaSpec           _spec;
      std::optional<MediaHandle> _mediaHandle;
    };
    Ref<Data> _sharedData;
  };
}

#endif // ZYPP_MEDIA_NG_LAZYMEDIAHANDLE_H
