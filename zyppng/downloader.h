/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPPNG_DOWNLOADER_H
#define ZYPPNG_DOWNLOADER_H


#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _ZyppContext ZyppContext;
typedef struct _ZyppManagedFile ZyppManagedFile;

/**
 * ZYPP_DOWNLOADER_ERROR:
 *
 * Error domain for download operations. Errors in this domain will
 * be from the #ZyppDownloaderError enumeration. See #GError for information
 * on error domains.
 */
#define ZYPP_DOWNLOADER_ERROR zypp_downloader_error_quark ()

/**
 * ZyppDownloaderError:
 * @ZYPP_DOWNLOADER_FAILED: Download failed for any reason, check result string
 */
typedef enum
{
  ZYPP_DOWNLOADER_FAILED
} ZyppDownloaderError;


#define ZYPP_TYPE_DOWNLOADER (zypp_downloader_get_type ())
G_DECLARE_FINAL_TYPE ( ZyppDownloader, zypp_downloader, ZYPP, DOWNLOADER, GObject )


GQuark zypp_downloader_error_quark();

/**
 * zypp_downloader_get_file_async:
 * @c: (nullable)
 * @cb: (scope async): a #GAsyncReadyCallback to call
 *   when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Convenience function to simply download a file from a URL
 */
void zypp_downloader_get_file_async ( ZyppDownloader *self, const gchar *url, const gchar *dest, GCancellable *c, GAsyncReadyCallback cb, gpointer user_data );


/**
 * zypp_downloader_get_file_finish:
 * @self: (in): a #ZyppDownloader
 * @result: where to place the result
 * @error: return location for a GError, or NULL
 *
 * Returns: (transfer full): #ZyppManagedFile for the downloaded file
 */
ZyppManagedFile * zypp_downloader_get_file_finish ( ZyppDownloader *self, GAsyncResult *result, GError **error );

G_END_DECLS

#ifdef  __cplusplus

#include <zyppng/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppDownloader, zypp_downloader, ZYPP, DOWNLOADER )
#endif

#endif // ZYPPNG_DOWNLOADER_H
