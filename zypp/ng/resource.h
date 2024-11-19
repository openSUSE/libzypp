/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ng/resource.h
 *
*/
#ifndef ZYPP_NG_RESOURCE_INCLUDED
#define ZYPP_NG_RESOURCE_INCLUDED

#include <string_view>
#include <deque>

#include <zypp-core/base/NonCopyable.h>
#include <zypp-core/base/ReferenceCounted.h>

#include <zypp-core/base/PtrTypes.h>
#include <zypp-core/zyppng/async/asyncop.h>
#include <zypp-core/zyppng/pipelines/expected.h>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (ContextBase);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (Timer);
  class RepoInfo;

  namespace detail {
    DEFINE_PTR_TYPE(ResourceLockData);
  }


  namespace resources {
    namespace repo {
      constexpr std::string_view REPO_RESOURCE_STR("/RepoManager/%1%");

      std::string REPO_RESOURCE( const RepoInfo &info );
    }
  }

  class ResourceAlreadyLockedException : public zypp::Exception
  {
  public:
    ResourceAlreadyLockedException( std::string ident );
    const std::string &ident() const;

  protected:
    std::ostream & dumpOn( std::ostream & str ) const override;
  private:
    std::string _ident;
  };

  class ResourceLockTimeoutException : public zypp::Exception
  {
  public:
    ResourceLockTimeoutException( std::string ident );
    const std::string &ident() const;

  protected:
    std::ostream & dumpOn( std::ostream & str ) const override;
  private:
    std::string _ident;
  };

  /**
   * Represents a reference to a shared or exclusive lock of a resource identified by
   * a unique string. Since libzypp supports async workflows we need to make
   * sure that we do not modify a resource multiple times.
   *
   * Copying the ResourceLockRef will increase the internal ref count on the lock
   * registered in the context. That way even a Exclusive Lock ownership can be
   * shared by subparts of a pipeline.
   */
  class ResourceLockRef {

    public:
      enum Mode{
        Shared,
        Exclusive
      };

      ResourceLockRef(detail::ResourceLockData_Ptr data);
      ~ResourceLockRef() = default;

      ResourceLockRef(const ResourceLockRef &) = default;
      ResourceLockRef &operator=(const ResourceLockRef &) = default;

      ResourceLockRef(ResourceLockRef && other) = default;
      ResourceLockRef &operator=(ResourceLockRef && other) = default;

      /**
       * Returns the context it belongs to
       */
      ContextBaseWeakRef lockContext() const;

      /**
       * Returns the lock identifier
       */
      const std::string &lockIdent() const;

      /**
       * Returns the lock mode
       */
      Mode mode() const;

  private:
      detail::ResourceLockData_Ptr _lockData;

  };

  ZYPP_FWD_DECL_TYPE_WITH_REFS(AsyncResourceLockReq);

  class AsyncResourceLockReq : public AsyncOp<expected<ResourceLockRef>>
  {
  public:
    AsyncResourceLockReq(std::string ident, ResourceLockRef::Mode m, uint timeout);

    AsyncResourceLockReq(const AsyncResourceLockReq &) = delete;
    AsyncResourceLockReq(AsyncResourceLockReq &&) = delete;
    AsyncResourceLockReq &operator=(const AsyncResourceLockReq &) = delete;
    AsyncResourceLockReq &operator=(AsyncResourceLockReq &&) = delete;

    ~AsyncResourceLockReq() override = default;

    void setFinished( expected<ResourceLockRef> &&lock );
    const std::string &ident() const;
    ResourceLockRef::Mode mode() const;

    SignalProxy<void (AsyncResourceLockReq &)> sigTimeout();

  private:
    void onTimeoutExceeded( Timer & );
    Signal<void (AsyncResourceLockReq &)> _sigTimeout;
    std::string _ident;
    ResourceLockRef::Mode _mode;
    TimerRef _timeout;
  };


  namespace detail {
    class ResourceLockData :  public zypp::base::ReferenceCounted, private zypp::base::NonCopyable
    {
    public:
      ContextBaseWeakRef _zyppContext;
      std::string        _resourceIdent;
      ResourceLockRef::Mode _mode = ResourceLockRef::Shared;

      // other requests waiting to lock the resource
      // only weak locks, calling code still needs to be able to cancel the pipeline by releasing it
      std::deque<std::weak_ptr<AsyncResourceLockReq>> _lockQueue;

      // ReferenceCounted interface
    protected:
      void unref_to(unsigned int) const override;
    };
  }
}

#endif // ZYPP_NG_RESOURCE_INCLUDED
