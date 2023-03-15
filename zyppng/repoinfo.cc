/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/repoinfo_p.h"
#include "private/infobase_p.h"

static void zypp_info_base_interface_init( ZyppInfoBaseInterface *iface )
{
  ZyppInfoBaseImpl<ZyppRepoInfo, zypp_repo_info_get_type >::init_interface( iface );
}

G_DEFINE_TYPE_WITH_CODE(ZyppRepoInfo, zypp_repo_info, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE ( ZYPP_TYPE_INFO_BASE, zypp_info_base_interface_init)
)


// define the GObject stuff
enum {
  PROP_0,
  PROP_NAME,
  PROP_ALIAS,
  PROP_ENABLED,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
zypp_repo_info_get_property (GObject    *object,
  guint       property_id,
  GValue     *value,
  GParamSpec *pspec)
{
  ZyppRepoInfo *self = ZYPP_REPOINFO (object);

  switch (property_id)
  {
    case PROP_NAME:
      g_value_set_string (value, self->_data._info.name().c_str());
      break;

    case PROP_ALIAS:
      g_value_set_string (value, self->_data._info.alias().c_str());
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, self->_data._info.enabled());
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

  switch (property_id)
  {
    case PROP_NAME:
      self->_data._info.setName(g_value_get_string (value));
      break;

    case PROP_ALIAS:
      self->_data._info.setAlias(g_value_get_string (value));
      break;

    case PROP_ENABLED:
      self->_data._info.setEnabled(g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void zypp_repo_info_init( ZyppRepoInfo *ctx )
{
  new ( &ctx->_data ) ZyppRepoInfo::Cpp();
}

static void zypp_repo_info_finalize (GObject *gobject)
{
  auto *ptr = ZYPP_REPOINFO (gobject);
  ptr->_data.~Cpp();

  // always call parent class finalizer
  G_OBJECT_CLASS (zypp_repo_info_parent_class)->finalize (gobject);
}

static void
zypp_repo_info_class_init (ZyppRepoInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = zypp_repo_info_get_property;
  object_class->set_property = zypp_repo_info_set_property;
  object_class->finalize     = zypp_repo_info_finalize;

  obj_properties[PROP_NAME] =
    g_param_spec_string ("name",
      "Name",
      "The name of the repository",
      "",
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  obj_properties[PROP_ALIAS] =
    g_param_spec_string ("alias",
      "Alias",
      "The alias of the repository",
      "",
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  obj_properties[PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
      "Enabled",
      "Whether the repository is enabled or not",
      FALSE,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
    N_PROPERTIES,
    obj_properties);
}

ZyppRepoInfo *zypp_repo_info_new( )
{
  return static_cast<ZyppRepoInfo *>(g_object_new (ZYPP_TYPE_REPOINFO, NULL));
}


ZyppRepoInfo *zypp_repo_info_wrap_cpp(zypp::RepoInfo &&info)
{
  auto newInf = zypp_repo_info_new ();
  newInf->_data._info = std::move(info);
  return newInf;
}

ZyppRepoInfoType zypp_repo_info_get_repo_type( ZyppRepoInfo *self )
{
  return static_cast<ZyppRepoInfoType>(self->_data._info.type ().toEnum ());
}
