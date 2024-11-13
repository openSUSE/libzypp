/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "error.h"
#include <zypp-glib/utils/GList>
#include <zypp-glib/utils/GLibMemory>
#include <new>

#include <zypp-core/base/Exception.h>

typedef struct
{
  zypp::Exception e;
} ZyppErrorPrivate;

static void
zypp_error_private_init ( ZyppErrorPrivate *priv )
{
  new (priv) ZyppErrorPrivate();
}

static void
zypp_error_private_copy (const ZyppErrorPrivate *src_priv, ZyppErrorPrivate *dest_priv)
{
  *dest_priv = *src_priv;
}

static void
zypp_error_private_clear (ZyppErrorPrivate *priv)
{
  priv->~ZyppErrorPrivate();
}

// This defines the zypp_error_get_private and zypp_error_quark functions.
G_DEFINE_EXTENDED_ERROR (ZyppError, zypp_error)

GList *zypp_error_get_history (GError *error)
{
  ZyppErrorPrivate *priv = zypp_error_get_private (error);
  g_return_val_if_fail (priv != NULL, NULL);
  // g_return_val_if_fail (error->code != MY_ERROR_BAD_REQUEST, NULL);

  zypp::glib::GCharContainer res;
  for ( auto i = priv->e.historyBegin (); i != priv->e.historyEnd (); i++ ) {
    res.push_back ( g_strdup( (*i).c_str ()) );
  }
  return res.take();
}

void zypp_error_from_exception ( GError **err, std::exception_ptr exception )
{
  try {
    std::rethrow_exception (exception);
  } catch ( const zypp::Exception &e ) {
    ZyppErrorPrivate *priv;
    g_set_error (err, ZYPP_ERROR, ZYPP_ERROR_GENERIC, "%s", e.msg ().c_str() );
    if (err != nullptr && *err != nullptr) {
      priv = zypp_error_get_private (*err);
      g_return_if_fail ( priv != nullptr );
      priv->e = e;
    }
  } catch ( std::exception &e ) {
    ZyppErrorPrivate *priv;
    g_set_error (err, ZYPP_ERROR, ZYPP_ERROR_GENERIC, "%s", e.what() );
    if (err != nullptr && *err != nullptr) {
      priv = zypp_error_get_private (*err);
      g_return_if_fail ( priv != nullptr );
      priv->e = zypp::Exception( e.what() );
    }
  } catch ( ... ) {
    ZyppErrorPrivate *priv;
    g_set_error (err, ZYPP_ERROR, ZYPP_ERROR_GENERIC, "Unknown exception." );
    if (err != nullptr && *err != nullptr) {
      priv = zypp_error_get_private (*err);
      g_return_if_fail ( priv != nullptr );
      priv->e = zypp::Exception( "Unknown exception" );
    }
  }
}
