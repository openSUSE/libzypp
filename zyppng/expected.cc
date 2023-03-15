/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "private/expected_p.h"

G_DEFINE_TYPE(ZyppExpected, zypp_expected, G_TYPE_OBJECT)

static void
zypp_expected_finalize (GObject *object)
{
  auto self = ZYPP_EXPECTED(object);
  if (zypp_expected_has_value(self)) {
    g_value_unset ( &self->value );
  } else {
    if ( self->error )
      g_error_free( self->error );
  }
  G_OBJECT_CLASS (zypp_expected_parent_class)->finalize (object);
}

static void
zypp_expected_class_init (ZyppExpectedClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = zypp_expected_finalize;
}

static void zypp_expected_init (ZyppExpected *self)
{
  self->value = G_VALUE_INIT;
  self->error = nullptr;
}

ZyppExpected *zypp_expected_new_value( const GValue *value )
{
  ZyppExpected *self = ZYPP_EXPECTED(g_object_new (ZYPP_TYPE_EXPECTED, NULL));
  g_value_init(&self->value, G_VALUE_TYPE(value));
  g_value_copy(value, &self->value);
  return self;
}

ZyppExpected *zypp_expected_new_error(GError *error)
{
  ZyppExpected *self = ZYPP_EXPECTED(g_object_new (ZYPP_TYPE_EXPECTED, NULL));
  self->error = error;
  return self;
}

gboolean zypp_expected_has_value(ZyppExpected *self)
{
  return self->error == NULL;
}

const GError *zypp_expected_get_error(ZyppExpected *self)
{
  if ( !self->error )
    return nullptr;

  return self->error;
}

const GValue *zypp_expected_get_value( ZyppExpected *self, GError **error )
{
  if ( !zypp_expected_has_value (self ) ) {
    if ( error && self->error ) *error = g_error_copy ( self->error );
    return nullptr;
  }
  return &self->value;
}

gboolean zypp_expected_has_error(ZyppExpected *self)
{
  return self->error != NULL;
}
