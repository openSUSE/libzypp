/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/repoinfo_p.h"
#include "private/context_p.h"
#include "private/infobase_p.h"


static void zypp_info_base_interface_init( ZyppInfoBaseInterface *iface );
ZYPP_DECLARE_GOBJECT_BOILERPLATE ( ZyppRepoInfo, zypp_repo_info )

G_DEFINE_TYPE_WITH_CODE(ZyppRepoInfo, zypp_repo_info, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE ( ZYPP_TYPE_INFO_BASE, zypp_info_base_interface_init)
  G_ADD_PRIVATE ( ZyppRepoInfo )
)


ZYPP_DEFINE_GOBJECT_BOILERPLATE  ( ZyppRepoInfo, zypp_repo_info, ZYPP, REPOINFO )
static void zypp_info_base_interface_init( ZyppInfoBaseInterface *iface )
{
  ZyppInfoBaseImpl<ZyppRepoInfo, zypp_repo_info_get_type, zypp_repo_info_get_private >::init_interface( iface );
}

#define ZYPP_REPO_INFO_D() \
  ZYPP_GLIB_WRAPPER_D( ZyppRepoInfo, zypp_repo_info )

// define the GObject stuff
enum {
  PROP_0,
  PROP_CPPOBJ,
  PROP_CONTEXT,
  PROP_NAME,
  PROP_ALIAS,
  PROP_ENABLED,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
zypp_repo_info_class_init (ZyppRepoInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS ( zypp_repo_info, object_class);

  obj_properties[PROP_CPPOBJ] = ZYPP_GLIB_ADD_CPPOBJ_PROP();

  obj_properties[PROP_CONTEXT] =
  g_param_spec_object( "zyppcontext",
                       "ZyppContext",
                       "The zypp context this repo manager belongs to",
                       zypp_context_get_type(),
                       GParamFlags( G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE) );

  obj_properties[PROP_NAME] =
    g_param_spec_string ("name",
      "Name",
      "The name of the repository",
      "",
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) );

  obj_properties[PROP_ALIAS] =
    g_param_spec_string ("alias",
      "Alias",
      "The alias of the repository",
      "",
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) );

  obj_properties[PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
      "Enabled",
      "Whether the repository is enabled or not",
      FALSE,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) );

  g_object_class_install_properties (object_class,
    N_PROPERTIES,
    obj_properties);
}

static void
zypp_repo_info_get_property (GObject    *object,
  guint       property_id,
  GValue     *value,
  GParamSpec *pspec)
{
  ZyppRepoInfo *self = ZYPP_REPOINFO (object);

  ZYPP_REPO_INFO_D();

  switch (property_id)
  {
    case PROP_NAME:
      g_value_set_string (value, d->_info.name().c_str());
      break;

    case PROP_ALIAS:
      g_value_set_string (value, d->_info.alias().c_str());
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, d->_info.enabled());
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
zypp_repo_info_set_property (GObject      *object,
  guint         property_id,
  const GValue *value,
  GParamSpec   *pspec)
{
  ZyppRepoInfo *self = ZYPP_REPOINFO (object);
  ZYPP_REPO_INFO_D();

  switch (property_id)
  {
    case PROP_CPPOBJ:
      g_return_if_fail( d->_constrProps ); // only if the constr props are still valid
      ZYPP_GLIB_SET_CPPOBJ_PROP( zypp::RepoInfo, value, d->_constrProps->_cppObj )
    case PROP_CONTEXT: {
      g_return_if_fail( d->_constrProps ); // only if the constr props are still valid
      ZyppContext *obj = ZYPP_CONTEXT(g_value_get_object( value ));
      g_return_if_fail( ZYPP_IS_CONTEXT(obj) );
      if ( d->_constrProps ) {
        d->_constrProps->_context = zypp::glib::ZyppContextRef( obj, zypp::glib::retain_object );
      }
      break;
    }
    case PROP_NAME:
      d->_info.setName(g_value_get_string (value));
      break;

    case PROP_ALIAS:
      d->_info.setAlias(g_value_get_string (value));
      break;

    case PROP_ENABLED:
      d->_info.setEnabled(g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void ZyppRepoInfoPrivate::initialize()
{
  g_return_if_fail ( _constrProps.has_value() );
  if ( _constrProps->_cppObj ) {
    _info = std::move( _constrProps->_cppObj.value() );
  } else {
    if ( !_constrProps->_context ) g_error("Context argument can not be NULL");
    _info = zypp::RepoInfo( zypp_context_get_cpp( _constrProps->_context.get() ) );
  }
  _constrProps.reset();
}

ZyppRepoInfo *zypp_repo_info_new(ZyppContext *context)
{
  g_return_val_if_fail( context != nullptr, nullptr );
  return static_cast<ZyppRepoInfo *>(g_object_new (ZYPP_TYPE_REPOINFO, "zyppcontext", context, NULL));
}

ZyppRepoInfo *zypp_wrap_cpp( zypp::RepoInfo info )
{
  return static_cast<ZyppRepoInfo *>(g_object_new (ZYPP_TYPE_REPOINFO, zypp::glib::internal::ZYPP_CPP_OBJECT_PROPERTY_NAME.data(), &info, NULL));
}

zypp::RepoInfo &zypp_repo_info_get_cpp( ZyppRepoInfo *self )
{
  ZYPP_REPO_INFO_D();
  return d->_info;
}

ZyppRepoInfoType zypp_repo_info_get_repo_type( ZyppRepoInfo *self )
{
  ZYPP_REPO_INFO_D();
  return static_cast<ZyppRepoInfoType>(d->_info.type ().toEnum ());
}
