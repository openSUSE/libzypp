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

#include <zypp-core/MirroredOrigin.h>
#include <zypp-media/ng/ProvideSpec>
#include <optional>


namespace zyppng
{
  template < class ProvideType >
  class LazyMediaHandle {
  public:
    using MediaHandle = typename ProvideType::MediaHandle;
    using ParentType  = ProvideType;

    friend ProvideType;

    LazyMediaHandle( Ref<ProvideType> provider, zypp::MirroredOrigin origin, ProvideMediaSpec spec )
      : _sharedData(
          std::make_shared<Data>(
            std::move(provider)
            ,std::move(origin)
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
        return _sharedData->_origin.authorities()[0].url();
      } else {
        return _sharedData->_mediaHandle->baseUrl();
      }
    }

    const zypp::MirroredOrigin &origin() const {
      return _sharedData->_origin;
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
      Data(Ref<ProvideType> &&provider, zypp::MirroredOrigin &&origin,
           ProvideMediaSpec &&spec)
        : _provider(std::move(provider)), _origin(std::move(origin)),
          _spec(std::move(spec)) {}

      Data(const Data &) = delete;
      Data(Data &&) = delete;
      Data &operator=(const Data &) = delete;
      Data &operator=(Data &&) = delete;

      WeakRef<ProvideType>       _provider;
      zypp::MirroredOrigin       _origin;
      ProvideMediaSpec           _spec;
      std::optional<MediaHandle> _mediaHandle;
    };
    Ref<Data> _sharedData;
  };
}

#endif // ZYPP_MEDIA_NG_LAZYMEDIAHANDLE_H
