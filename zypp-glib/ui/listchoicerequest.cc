/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/listchoicerequest_p.h"
#include "private/userrequest_p.h"

#include <zypp-glib/private/globals_p.h>
#include <zypp-glib/ui/zyppenums-ui.h>

ZyppListChoiceOption * zypp_wrap_cpp( zyppng::ListChoiceRequest::Choice obj )
{
  std::unique_ptr<ZyppListChoiceOption> ptr( new ZyppListChoiceOption() );
  ptr->_cppObj = std::move(obj);
  return ptr.release();
}

ZyppListChoiceOption * zypp_list_choice_option_copy ( ZyppListChoiceOption *r )
{
  return zypp_wrap_cpp(r->_cppObj);
}

void zypp_list_choice_option_free ( ZyppListChoiceOption *r )
{
  delete r;
}


G_DEFINE_BOXED_TYPE (ZyppListChoiceOption, zypp_list_choice_option,
                     zypp_list_choice_option_copy,
                     zypp_list_choice_option_free)


const char * zypp_list_choice_option_get_label( ZyppListChoiceOption *self )
{
  return self->_cppObj.opt.c_str ();
}

const char * zypp_list_choice_option_get_detail( ZyppListChoiceOption *self )
{
  return self->_cppObj.detail.c_str ();
}

static void user_request_if_init( ZyppUserRequestInterface *iface );

ZYPP_DECLARE_GOBJECT_BOILERPLATE ( ZyppListChoiceRequest, zypp_list_choice_request )

G_DEFINE_TYPE_WITH_CODE(ZyppListChoiceRequest, zypp_list_choice_request, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE ( ZYPP_TYPE_USER_REQUEST, user_request_if_init )
  G_ADD_PRIVATE ( ZyppListChoiceRequest )
)

ZYPP_DEFINE_GOBJECT_BOILERPLATE ( ZyppListChoiceRequest, zypp_list_choice_request, ZYPP, LIST_CHOICE_REQUEST )

#define ZYPP_D() \
  ZYPP_GLIB_WRAPPER_D( ZyppListChoiceRequest, zypp_list_choice_request )

// define the GObject stuff
enum {
  PROP_CPPOBJ  = 1,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void user_request_if_init( ZyppUserRequestInterface *iface )
{
  ZyppUserRequestImpl<ZyppListChoiceRequest, zypp_list_choice_request_get_type, zypp_list_choice_request_get_private >::init_interface( iface );
}

static void zypp_list_choice_request_class_init (ZyppListChoiceRequestClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS ( zypp_list_choice_request, gobject_class );
  obj_properties[PROP_CPPOBJ] = ZYPP_GLIB_ADD_CPPOBJ_PROP();

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

static void
zypp_list_choice_request_get_property (GObject    *object,
  guint       property_id,
  GValue     *value,
  GParamSpec *pspec)
{
  g_return_if_fail( ZYPP_IS_LIST_CHOICE_REQUEST (object) );
  switch (property_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
zypp_list_choice_request_set_property (GObject      *object,
  guint         property_id,
  const GValue *value,
  GParamSpec   *pspec)
{
  g_return_if_fail( ZYPP_IS_LIST_CHOICE_REQUEST (object) );
  ZyppListChoiceRequest *self = ZYPP_LIST_CHOICE_REQUEST (object);

  ZYPP_D();

  switch (property_id)
  {
    case PROP_CPPOBJ: {
      g_return_if_fail( d->_constrProps ); // only if the constr props are still valid
      ZYPP_GLIB_SET_CPPOBJ_PROP( zyppng::ListChoiceRequestRef, value, d->_constrProps->_cppObj );
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

zyppng::ListChoiceRequestRef &zypp_list_choice_request_get_cpp( ZyppListChoiceRequest *self )
{
  ZYPP_D();
  return d->_cppObj;
}

const char * zypp_list_choice_request_get_label( ZyppListChoiceRequest *self )
{
  ZYPP_D();
  return d->_cppObj->label().c_str();
}

GList  *zypp_list_choice_request_get_options ( ZyppListChoiceRequest *self )
{
  ZYPP_D();
  zypp::glib::GListContainer<ZyppListChoiceOption> opts;
  for ( const auto &answer : d->_cppObj->answers() ) {
    opts.push_back ( zypp_wrap_cpp(answer) );
  }
  return opts.take();
}

void zypp_list_choice_request_set_choice ( ZyppListChoiceRequest *self, guint choice )
{
  ZYPP_D();
  d->_cppObj->setChoice(choice);
}

guint zypp_list_choice_request_get_choice  ( ZyppListChoiceRequest *self )
{
  ZYPP_D();
  return d->_cppObj->choice();
}

guint zypp_list_choice_request_get_default( ZyppListChoiceRequest *self )
{
  ZYPP_D();
  return d->_cppObj->defaultAnswer();
}

