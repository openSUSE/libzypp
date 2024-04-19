/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_MEDIASETFACADE_INCLUDED
#define ZYPP_NG_MEDIASETFACADE_INCLUDED

#include <zypp-core/base/PtrTypes.h>
#include <zypp-media/ng/Provide>
#include <zypp-media/ng/LazyMediaHandle>
#include <zypp/MediaSetAccess.h>
#include <zypp/media/MediaManager.h>


#include <vector>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS ( MediaSyncFacade );
  DEFINE_PTR_TYPE(AttachedSyncMediaInfo);


  class SyncMediaHandle {
  public:

    using ParentType = MediaSyncFacade;

    SyncMediaHandle();
    SyncMediaHandle( AttachedSyncMediaInfo_Ptr dataPtr );
    MediaSyncFacadeRef parent() const;
    bool isValid () const;
    const zypp::Url &baseUrl() const;
    const std::optional<zypp::Pathname> &localPath() const;
    const AttachedSyncMediaInfo &info ()const;

  private:
    AttachedSyncMediaInfo_Ptr _data;
  };

  /*!
   * A Facade class that mimics the behavior of the Provide
   * class just in a sync way. Meaning every operation will finish immediately
   * instead of returning a \ref AsyncOp.
   */
  class ZYPP_API MediaSyncFacade : public Base
  {
    ZYPP_ADD_CREATE_FUNC(MediaSyncFacade);
  public:

    friend class AttachedSyncMediaInfo;

    using MediaHandle     = SyncMediaHandle;
    using LazyMediaHandle = ::zyppng::LazyMediaHandle<MediaSyncFacade>;

    class Res {
      public:

      Res( MediaHandle hdl, zypp::ManagedFile file );

      /*!
       * Returns the path to the provided file
       */
      const zypp::Pathname file () const;

      /*!
       * Returns a reference to the internally used managed file instance.
       * \note If you obtain this for a file that is inside the providers working directory ( e.g. a provide result for a download ),
       *       the continued use after the Provide instance was relased is undefined behaviour and not supported!
       */
      const zypp::ManagedFile & asManagedFile () const {
        return _res;
      }

    private:
      zypp::ManagedFile _res;
      MediaHandle _provideHandle;
    };

    ZYPP_DECL_PRIVATE_CONSTR ( MediaSyncFacade );
    ~MediaSyncFacade() override;

    expected<LazyMediaHandle> prepareMedia ( const std::vector<zypp::Url> &urls, const ProvideMediaSpec &request );
    expected<LazyMediaHandle> prepareMedia ( const zypp::Url &url, const ProvideMediaSpec &request );

    expected<MediaHandle> attachMediaIfNeeded( LazyMediaHandle lazyHandle );
    expected<MediaHandle> attachMedia( const std::vector<zypp::Url> &urls, const ProvideMediaSpec &request );
    expected<MediaHandle> attachMedia( const zypp::Url &url, const ProvideMediaSpec &request );

    expected<Res> provide(  const std::vector<zypp::Url> &urls, const ProvideFileSpec &request );
    expected<Res> provide(  const zypp::Url &url, const ProvideFileSpec &request );
    expected<Res> provide(  const MediaHandle &attachHandle, const zypp::Pathname &fileName, const ProvideFileSpec &request );
    expected<Res> provide(  const LazyMediaHandle &attachHandle, const zypp::Pathname &fileName, const ProvideFileSpec &request );


    /*!
     * Schedules a job to calculate the checksum for the given file
     */
    expected<zypp::CheckSum> checksumForFile ( const zypp::Pathname &p, const std::string &algorithm );

    /*!
     * Schedules a copy job to copy a file from \a source to \a target
     */
    expected<zypp::ManagedFile> copyFile ( const zypp::Pathname &source, const zypp::Pathname &target );
    expected<zypp::ManagedFile> copyFile ( Res source, const zypp::Pathname &target );

    static auto copyResultToDest ( MediaSyncFacadeRef provider, const zypp::Pathname &targetPath ) {
      return [ providerRef=std::move(provider), targetPath = targetPath ]( Res &&file ){
        zypp::filesystem::assert_dir( targetPath.dirname () );
        return providerRef->copyFile( std::move(file), targetPath );
      };
    }

  protected:
    void releaseMedium ( const AttachedSyncMediaInfo *ptr );

  private:
    std::vector<zypp::Url> sanitizeUrls(const std::vector<zypp::Url> &urls) const;
    std::vector<AttachedSyncMediaInfo_Ptr> _attachedMedia;
  };

  using SyncProvideRes = MediaSyncFacade::Res;

  //template <bool async>
  //using MediaFacade = std::conditional_t<async, MediaAsyncFacade, MediaSyncFacade>;
}



#endif
