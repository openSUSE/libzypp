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

#include <zypp-core/ng/base/base.h>
#include <zypp-core/base/PtrTypes.h>
#include <zypp-core/MirroredOrigin.h>
#include <zypp-core/ManagedFile.h>
#include <zypp-media/ng/LazyMediaHandle>
#include <zypp/media/MediaManager.h>
#include <zypp-core/ng/pipelines/expected.h>

#include <vector>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS ( Provide );
  DEFINE_PTR_TYPE(AttachedMediaInfo);


  class ProvideMediaHandle {
  public:

    using ParentType = Provide;

    ProvideMediaHandle();
    ProvideMediaHandle( AttachedMediaInfo_Ptr dataPtr );
    ProvideRef parent() const;
    bool isValid () const;
    const zypp::Url &baseUrl() const;
    const std::optional<zypp::Pathname> &localPath() const;
    const AttachedMediaInfo &info ()const;

  private:
    AttachedMediaInfo_Ptr _data;
  };

  class ProvideRes {
    public:

    ProvideRes( ProvideMediaHandle hdl, zypp::ManagedFile file );

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

    /*!
     * Returns a reference to the currently held media handle, this can be a invalid handle
     */
    const ProvideMediaHandle &mediaHandle () const {
      return _provideHandle;
    }

#if 0

    Disabled for now so we hit a compile error, since we currently have no way to get those values from the old media backend.

    /*!
     * The URL this ressource was provided from, can be empty
     */
    const zypp::Url &resourceUrl () const {
      return _resourceUrl;
    }
#endif

    /*!
     * All headers that were received from the worker when sending the result
     * \note will always be empty for the sync media backend.
     */
    const HeaderValueMap &headers () const {
      return _hdrs;
    }

  private:
    zypp::ManagedFile _res;
    ProvideMediaHandle _provideHandle;
    // zypp::Url _resourceUrl;
    HeaderValueMap _hdrs;
  };

  /*!
   * A Facade class that mimics the behavior of the Provide
   * class just in a sync way. Meaning every operation will finish immediately
   * instead of returning a \ref AsyncOp.
   */
  class ZYPP_API Provide : public Base
  {
    ZYPP_ADD_CREATE_FUNC(Provide)
  public:

    friend class AttachedMediaInfo;

    using MediaHandle     = ProvideMediaHandle;
    using LazyMediaHandle = ::zyppng::LazyMediaHandle<Provide>;
    using Res = ProvideRes;

    ZYPP_DECL_PRIVATE_CONSTR ( Provide );
    ~Provide() override;

    expected<LazyMediaHandle> prepareMedia ( const zypp::MirroredOrigin &origin, const ProvideMediaSpec &request );
    expected<LazyMediaHandle> prepareMedia ( const zypp::Url &url, const ProvideMediaSpec &request );

    expected<MediaHandle> attachMediaIfNeeded( LazyMediaHandle lazyHandle );
    expected<MediaHandle> attachMedia( const zypp::MirroredOrigin &origin, const ProvideMediaSpec &request );
    expected<MediaHandle> attachMedia( const zypp::Url &url, const ProvideMediaSpec &request );

    expected<Res> provide(  const zypp::MirroredOrigin &origin, const ProvideFileSpec &request );
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

    static auto copyResultToDest ( ProvideRef provider, const zypp::Pathname &targetPath ) {
      return [ providerRef=std::move(provider), targetPath = targetPath ]( Res &&file ){
        zypp::filesystem::assert_dir( targetPath.dirname () );
        return providerRef->copyFile( std::move(file), targetPath );
      };
    }

  protected:
    void releaseMedium ( const AttachedMediaInfo *ptr );

  private:
    zypp::MirroredOrigin sanitizeUrls(const zypp::MirroredOrigin &origin) const;
    std::vector<AttachedMediaInfo_Ptr> _attachedMedia;
  };

  using ProvideRes = Provide::Res;

  //template <bool async>
  //using MediaFacade = std::conditional_t<async, MediaAsyncFacade, Provide>;
}



#endif
