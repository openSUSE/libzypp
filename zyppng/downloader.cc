/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/downloader_p.h"
#include "private/context_p.h"

#include <zypp-media/ng/ProvideSpec>
#include <zypp-core/zyppng/pipelines/expected.h>
#include <zyppng/utils/GLibMemory>

G_DEFINE_TYPE(ZyppDownloader, zypp_downloader, G_TYPE_OBJECT)

G_DEFINE_QUARK (zypp-downloader-error-quark, zypp_downloader_error)

namespace {
  struct AsyncData {
    GTask *myTask = nullptr; //weak ptr to task, does not take a ref
    std::shared_ptr<zyppng::AsyncOpBase> op; //the operation we are running
  };
}

/*!
 * Helper function to copy a ProvideRes underlying file to a target destination, keeping the ProvideRes alive until the copy operation has finished
 */
zyppng::AsyncOpRef<zyppng::expected<zypp::ManagedFile>> copyProvideResToFile ( zyppng::ProvideRef prov, zyppng::ProvideRes &&res, const zypp::Pathname &targetDir )
{
  using namespace zyppng::operators;

  auto fName = res.file();
  return prov->copyFile( fName, targetDir/fName.basename() )
    | [ resSave = std::move(res) ] ( auto &&result ) {
      // callback lambda to keep the ProvideRes reference around until the op is finished,
      // if the op fails the callback will be cleaned up and so the reference
      return result;
    };
}

static void zypp_downloader_dispose (GObject *gobject)
{
  /* In dispose(), you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a
   * reference.
   */

  // always call parent class dispose
  G_OBJECT_CLASS (zypp_downloader_parent_class)->dispose (gobject);
}

static void zypp_downloader_finalize (GObject *gobject)
{
  ZyppDownloader *ptr = ZYPP_DOWNLOADER(gobject);
  g_clear_weak_pointer( &ptr->_data.context );
  ptr->_data.~Cpp();

  // always call parent class finalizer
  G_OBJECT_CLASS (zypp_downloader_parent_class)->finalize (gobject);
}

void zypp_downloader_class_init( ZyppDownloaderClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = zypp_downloader_dispose;
  object_class->finalize = zypp_downloader_finalize;
}

void zypp_downloader_init( ZyppDownloader *ctx )
{
  new ( &ctx->_data ) ZyppDownloader::Cpp();
}

ZyppDownloader *zypp_downloader_new( ZyppContext *ctx )
{
  ZyppDownloader *obj = ZYPP_DOWNLOADER(g_object_new( zypp_downloader_get_type(), NULL ));
  g_set_weak_pointer( &obj->_data.context, ctx );
  obj->_data._provider = zyppng::Provide::create("/tmp/testdir");
  return obj;
}

void zypp_downloader_get_file_async ( ZyppDownloader *self, const gchar *url, const gchar *dest, GCancellable *c, GAsyncReadyCallback cb, gpointer user_data )
{
  auto task = zyppng::util::share_gobject( g_task_new( self, c, cb, user_data ) );
  auto data = new AsyncData();

  using namespace zyppng::operators;

  data->op = self->_data._provider->provide( zypp::Url(url), zyppng::ProvideFileSpec() )
             | mbind ( std::bind( &copyProvideResToFile, self->_data._provider, std::placeholders::_1, zypp::Pathname(dest) ) )
             | [ task ]( zyppng::expected<zypp::ManagedFile> &&res ){

                auto dl = ZYPP_DOWNLOADER(g_task_get_source_object( task.get() ));

                auto i = std::find( dl->_data._runningTasks.begin(), dl->_data._runningTasks.end(), task );
                if ( i != dl->_data._runningTasks.end() ) {
                  dl->_data._runningTasks.erase(i);
                } else {
                  WAR << "Task was not in the list of currently running tasks, thats a bug" << std::endl;
                }

                if ( !res ) {
                  G_FILE_ERROR;
                  g_task_return_error( task.get(), g_error_new( ZYPP_DOWNLOADER_ERROR, ZYPP_DOWNLOADER_FAILED, "The download failed" ) );
                  return 0;
                }

                // give da file to the caller
                res->resetDispose();
                g_task_return_pointer( task.get(), g_strdup( res->value().c_str() ), g_free );


                // dummy return
                return 0;
             };

  self->_data._provider->start();

  g_task_set_task_data( task.get(), data, zyppng::util::g_destroy_notify_delete<AsyncData> );
  self->_data._runningTasks.push_back(task);
}

gchar * zypp_downloader_get_file_finish ( ZyppDownloader *self,
                                          GAsyncResult  *result,
                                          GError **error )
{
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  return reinterpret_cast<gchar *>(g_task_propagate_pointer (G_TASK (result), error));
}
