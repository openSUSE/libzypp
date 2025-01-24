/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/progressobserver_p.h"

#include <zypp/ng/userrequest.h>
#include <zypp-glib/ui/userrequest.h>
#include <zypp-glib/ui/booleanchoicerequest.h>
#include <zypp-glib/ui/inputrequest.h>
#include <zypp-glib/ui/listchoicerequest.h>
#include <zypp-glib/ui/showmessagerequest.h>

#include <zypp-core/base/LogControl.h>
#include <zypp-core/base/String.h>

G_DEFINE_FINAL_TYPE_WITH_PRIVATE( ZyppProgressObserver, zypp_progress_observer, G_TYPE_OBJECT )

/* Signals */
typedef enum {
  SIG_FINISHED = 1,
  SIG_STARTED,
  SIG_NEW_CHILD,
  SIG_EVENT,
  LAST_SIGNAL
} ZyppProgressObserverSignals;

static guint signals[LAST_SIGNAL] = { 0, };

typedef enum
{
  PROP_CPPOBJ = 1,
  PROP_LABEL,
  PROP_BASE_STEPS,
  PROP_STEPS,
  PROP_VALUE,
  PROP_PROGRESS,
  N_PROPERTIES
} ZyppProgressObserverProperty;

#define ZYPP_PROGRESS_OBSERVER_D() \
  ZYPP_GLIB_WRAPPER_D( ZyppProgressObserver, zypp_progress_observer )


static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void zypp_progress_observer_constructed  (GObject *gobject);
static void zypp_progress_observer_finalize     (GObject *gobject);
static void zypp_progress_observer_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void zypp_progress_observer_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

static void zypp_progress_observer_class_init (ZyppProgressObserverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->constructed  = zypp_progress_observer_constructed;
  object_class->finalize     = zypp_progress_observer_finalize;
  object_class->get_property = zypp_progress_observer_get_property;
  object_class->set_property = zypp_progress_observer_set_property;

  {
    std::vector<GType> signal_parms = {};
    signals[SIG_STARTED] =
      g_signal_newv ("start",
                     G_TYPE_FROM_CLASS (klass),
                    (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS),
                    NULL /* closure */,
                    NULL /* accumulator */,
                    NULL /* accumulator data */,
                    NULL /* C marshaller */,
                    G_TYPE_NONE /* return_type */,
                    signal_parms.size() /* n_params */,
                    signal_parms.data() );
  }

  {
    std::vector<GType> signal_parms = {};
    signals[SIG_FINISHED] =
      g_signal_newv ("finished",
                     G_TYPE_FROM_CLASS (klass),
                    (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS),
                    NULL /* closure */,
                    NULL /* accumulator */,
                    NULL /* accumulator data */,
                    NULL /* C marshaller */,
                    G_TYPE_NONE /* return_type */,
                    signal_parms.size() /* n_params */,
                    signal_parms.data() );
  }

  {
    std::vector<GType> signal_parms = { ZYPP_TYPE_PROGRESS_OBSERVER };
    signals[SIG_NEW_CHILD] =
      g_signal_newv ("new-subtask",
        G_TYPE_FROM_CLASS (klass),
        (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS),
        NULL /* closure */,
        NULL /* accumulator */,
        NULL /* accumulator data */,
        NULL /* C marshaller */,
        G_TYPE_NONE /* return_type */,
        signal_parms.size() /* n_params */,
        signal_parms.data() );
  }

  {
    std::vector<GType> signal_parms = { ZYPP_TYPE_USER_REQUEST };
    signals[SIG_EVENT] =
      g_signal_newv ("event",
                     G_TYPE_FROM_CLASS (klass),
                    (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS),
                    NULL /* closure */,
                    NULL /* accumulator */,
                    NULL /* accumulator data */,
                    NULL /* C marshaller */,
                    G_TYPE_NONE /* return_type */,
                    signal_parms.size() /* n_params */,
                    signal_parms.data() );
  }

  obj_properties[PROP_CPPOBJ] = ZYPP_GLIB_ADD_CPPOBJ_PROP();

  obj_properties[PROP_LABEL] = g_param_spec_string (
      "label",
      nullptr,
      "The label of the corresponding Task.",
      nullptr  /* default value */,
      GParamFlags( G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY ));

  obj_properties[PROP_BASE_STEPS] = g_param_spec_int (
    "base-steps",
    nullptr,
    "The number of steps for this task.",
    0.0,          /* minimum */
    G_MAXINT,     /* max */
    0,            /* default */
    GParamFlags( G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY  ));

  obj_properties[PROP_STEPS] = g_param_spec_double (
      "steps",
      nullptr,
      "The number of steps for this task and weighted subtasks.",
      0.0,          /* minimum */
      G_MAXDOUBLE,  /* max */
      0,            /* default */
      GParamFlags( G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY  ));

  obj_properties[PROP_VALUE] = g_param_spec_double (
      "value",
      nullptr,
      "The progress tasks's current step counter value.",
      0.0,          /* minimum */
      G_MAXDOUBLE,  /* max */
      0,            /* default */
      GParamFlags( G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY  ));

  obj_properties[PROP_PROGRESS] = g_param_spec_double (
    "progress",
    nullptr,
    "The progress tasks's current step percentage value.",
    0.0,          /* minimum */
    100.0,  /* max */
    0,            /* default */
    GParamFlags( G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY  ));

  g_object_class_install_properties ( object_class, N_PROPERTIES, obj_properties );
}

static void zypp_progress_observer_init ( ZyppProgressObserver *self )
{
  ZYPP_GLIB_CONSTRUCT_WRAPPER_PRIVATE(ZyppProgressObserver, zypp_progress_observer);
}

static void zypp_progress_observer_constructed  ( GObject *object )
{
  g_return_if_fail( ZYPP_IS_PROGRESS_OBSERVER(object) );
  ZyppProgressObserver *self = ZYPP_PROGRESS_OBSERVER (object);
  ZYPP_GLIB_INIT_WRAPPER_PRIVATE(ZyppProgressObserver, zypp_progress_observer);
}

static void
zypp_progress_observer_set_property (GObject      *object,
  guint         property_id,
  const GValue *value,
  GParamSpec   *pspec)
{
  g_return_if_fail( ZYPP_IS_PROGRESS_OBSERVER(object) );
  ZyppProgressObserver *self = ZYPP_PROGRESS_OBSERVER (object);
  ZYPP_PROGRESS_OBSERVER_D();

  switch ((ZyppProgressObserverProperty)property_id ) {
    case PROP_CPPOBJ:
      g_return_if_fail( d->_constrProps ); // only if the constr props are still valid
      ZYPP_GLIB_SET_CPPOBJ_PROP( zyppng::ProgressObserverRef, value, d->_constrProps->_cppObj )
    case PROP_LABEL: {
      zypp_progress_observer_set_label( self, g_value_get_string ( value ) );
      break;
    }
    case PROP_BASE_STEPS: {
      zypp_progress_observer_set_base_steps( self, g_value_get_int ( value ) );
      break;
    }
    case PROP_VALUE: {
      zypp_progress_observer_set_current ( self, g_value_get_double ( value ) );
      break;
    }
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
zypp_progress_observer_get_property (GObject    *object,
  guint       property_id,
  GValue     *value,
  GParamSpec *pspec)
{
  g_return_if_fail( ZYPP_IS_PROGRESS_OBSERVER(object) );
  ZyppProgressObserver *self = ZYPP_PROGRESS_OBSERVER (object);
  ZYPP_PROGRESS_OBSERVER_D();

  switch ((ZyppProgressObserverProperty)property_id )
  {
    case PROP_LABEL: {
      g_value_set_string ( value, d->_cppObj->label().c_str() );
      break;
    }
    case PROP_BASE_STEPS: {
      g_value_set_int ( value, zypp_progress_observer_get_steps(self) );
      break;
    }
    case PROP_STEPS: {
      g_value_set_double ( value, zypp_progress_observer_get_steps(self) );
      break;
    }
    case PROP_VALUE: {
      g_value_set_double ( value, zypp_progress_observer_get_current(self) );
      break;
    }
    case PROP_PROGRESS: {
      g_value_set_double ( value, zypp_progress_observer_get_progress(self) );
      break;
    }
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void zypp_progress_observer_finalize (GObject *gobject)
{
  ZyppProgressObserver *self = ZYPP_PROGRESS_OBSERVER(gobject);
  ZYPP_PROGRESS_OBSERVER_D();

  // make sure our children don't try to send data to us anymore.
  for ( ZyppProgressObserver *child : d->_children )
    g_signal_handlers_disconnect_by_data( child, self );

  d->~ZyppProgressObserverPrivate();

  // always call parent class finalizer
  G_OBJECT_CLASS (zypp_progress_observer_parent_class)->finalize (gobject);
}


static void child_finished ( ZyppProgressObserver *sender, ZyppProgressObserver *self )
{
  ZYPP_PROGRESS_OBSERVER_D();
  //forget about it
  g_signal_handlers_disconnect_by_data( sender, self );

  auto i =  d->_children.find( sender );
  if ( i != d->_children.end() )
    d->_children.remove(i);
}

// here in the C world we only need to track the childs lifetime, progress calculation
// and tracking is done in the C++ world anyway
static void zypp_progress_observer_reg_child ( ZyppProgressObserver *self, zypp::glib::ZyppProgressObserverRef &&child )
{
  if ( !child )
    return;

  ZYPP_PROGRESS_OBSERVER_D();

  if ( d->_children.find( child.get() )!= d->_children.end() )
    return;

  // first things first: make sure the ref is in our list so its cleaned up
  d->_children.push_back ( child.get() );

  // the ref is now tracked by the list
  auto rawChild = child.detach();

  // once the child is finished, forget about it
  g_signal_connect ( rawChild, "finished", G_CALLBACK( child_finished ), self );
}

void zypp_progress_observer_add_subtask( ZyppProgressObserver *self, ZyppProgressObserver *newChild, gfloat weight )
{
  ZYPP_PROGRESS_OBSERVER_D();
  auto dChild = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( newChild ));
  d->_cppObj->registerSubTask ( dChild->_cppObj, weight );
}

const gchar *zypp_progress_observer_get_label ( ZyppProgressObserver *self )
{
  ZYPP_PROGRESS_OBSERVER_D();
  return d->_cppObj->label().c_str();
}

gdouble zypp_progress_observer_get_steps(ZyppProgressObserver *self)
{
  ZYPP_PROGRESS_OBSERVER_D();
  return d->_cppObj->steps();
}

gint zypp_progress_observer_get_base_steps(ZyppProgressObserver *self)
{
  ZYPP_PROGRESS_OBSERVER_D();
  return d->_cppObj->baseSteps();
}

gdouble zypp_progress_observer_get_current  ( ZyppProgressObserver *self )
{
  ZYPP_PROGRESS_OBSERVER_D();
  return d->_cppObj->current();
}

gdouble zypp_progress_observer_get_progress  ( ZyppProgressObserver *self )
{
  ZYPP_PROGRESS_OBSERVER_D();
  return d->_cppObj->progress ();
}

void zypp_progress_observer_set_label(ZyppProgressObserver *self, const gchar *label)
{
  ZYPP_PROGRESS_OBSERVER_D();
  d->_cppObj->setLabel( zypp::str::asString(label) );
}

void zypp_progress_observer_set_base_steps (ZyppProgressObserver *self, gint max )
{
  ZYPP_PROGRESS_OBSERVER_D();
  d->_cppObj->setBaseSteps(max);
}

void zypp_progress_observer_set_current  (ZyppProgressObserver *self, gdouble value )
{
  ZYPP_PROGRESS_OBSERVER_D();
  d->_cppObj->setCurrent(value);
}

void zypp_progress_observer_inc(ZyppProgressObserver *self, gint increase)
{
  ZYPP_PROGRESS_OBSERVER_D();
  d->_cppObj->inc(increase, {});
}

void zypp_progress_observer_set_finished(ZyppProgressObserver *self)
{
  ZYPP_PROGRESS_OBSERVER_D();
  d->_cppObj->setFinished();
}

const GList *zypp_progress_observer_get_children(ZyppProgressObserver *self)
{
  ZYPP_PROGRESS_OBSERVER_D();
  return d->_children.get();
}

void ZyppProgressObserverPrivate::initializeCpp() {

  if ( _constrProps && _constrProps->_cppObj ) {
    _cppObj = std::move( _constrProps->_cppObj );
  } else {
    _cppObj = zyppng::ProgressObserver::create();
  }
  _constrProps.reset();

  _signalConns.insert ( _signalConns.end(), {
     _cppObj->connectFunc( &zyppng::ProgressObserver::sigLabelChanged, [this]( auto &, const std::string & ){
       g_object_notify_by_pspec ( G_OBJECT(_gObject), obj_properties[PROP_LABEL] );
     }),
     _cppObj->connectFunc( &zyppng::ProgressObserver::sigStepsChanged, [this]( auto &, double ){
       g_object_notify_by_pspec ( G_OBJECT(_gObject), obj_properties[PROP_STEPS] );
     }),
     _cppObj->connectFunc( &zyppng::ProgressObserver::sigValueChanged, [this]( auto &, double ){
       g_object_notify_by_pspec ( G_OBJECT(_gObject), obj_properties[PROP_VALUE] );
     }),
     _cppObj->connectFunc( &zyppng::ProgressObserver::sigProgressChanged, [this]( auto &, double ){
       g_object_notify_by_pspec ( G_OBJECT(_gObject), obj_properties[PROP_PROGRESS] );
     }),
     _cppObj->connectFunc( &zyppng::ProgressObserver::sigStarted, [this]( zyppng::ProgressObserver &sender ){
      g_signal_emit (_gObject, signals[SIG_STARTED], 0);
    }),
     _cppObj->connectFunc( &zyppng::ProgressObserver::sigFinished, [this]( zyppng::ProgressObserver &sender, zyppng::ProgressObserver::FinishResult ){
       g_signal_emit (_gObject, signals[SIG_FINISHED], 0);
     }),
     _cppObj->connectFunc( &zyppng::ProgressObserver::sigNewSubprogress, [this]( auto &, zyppng::ProgressObserverRef newRef ){
       auto wrapped = zypp::glib::zypp_wrap_cpp<ZyppProgressObserver>( newRef );
       zypp_progress_observer_reg_child ( _gObject, zypp::glib::ZyppProgressObserverRef(wrapped) );
       g_signal_emit( _gObject, signals[SIG_NEW_CHILD], 0, wrapped.get() );
     }),
      _cppObj->connectFunc( &zyppng::ProgressObserver::sigEvent, [this]( zyppng::ProgressObserver &s, zyppng::UserRequestRef req ) {
           switch( req->type() )  {
             case zyppng::UserRequestType::Invalid: {
               ERR << "Ignoring user request with type Invalid, this is a bug!" << std::endl;
               return; //ignore
             }
             case zyppng::UserRequestType::Message: {
               auto actualReq = std::dynamic_pointer_cast<zyppng::ShowMessageRequest>(req);
               auto gObjMsg = zypp::glib::zypp_wrap_cpp<ZyppShowMsgRequest>(actualReq);
               g_signal_emit (_gObject, signals[SIG_EVENT], 0, gObjMsg.get(), nullptr );
               return;
             }
             case zyppng::UserRequestType::ListChoice: {
               auto actualReq = std::dynamic_pointer_cast<zyppng::ListChoiceRequest>(req);
               auto gObjMsg = zypp::glib::zypp_wrap_cpp<ZyppListChoiceRequest>(actualReq);
               g_signal_emit (_gObject, signals[SIG_EVENT], 0, gObjMsg.get(), nullptr );
               return;
             }
             case zyppng::UserRequestType::BooleanChoice: {
               auto actualReq = std::dynamic_pointer_cast<zyppng::BooleanChoiceRequest>(req);
               auto gObjMsg = zypp::glib::zypp_wrap_cpp<ZyppBooleanChoiceRequest>(actualReq);
               g_signal_emit (_gObject, signals[SIG_EVENT], 0, gObjMsg.get(), nullptr );
               return;
             }
             case zyppng::UserRequestType::InputRequest: {
              auto actualReq = std::dynamic_pointer_cast<zyppng::InputRequest>(req);
              auto gObjMsg = zypp::glib::zypp_wrap_cpp<ZyppInputRequest>(actualReq);
              g_signal_emit (_gObject, signals[SIG_EVENT], 0, gObjMsg.get(), nullptr );
              return;
             }
             case zyppng::UserRequestType::Custom: {
               ERR << "Custom user requests can not be wrapped with glib!" << std::endl;
               return; //ignore
             }
           }
      })
   });

  for ( const auto &chld : _cppObj->children () ) {
    zypp_progress_observer_reg_child( _gObject, zypp::glib::zypp_wrap_cpp<ZyppProgressObserver>(chld) );
  }
}

zyppng::ProgressObserverRef zypp_progress_observer_get_cpp( ZyppProgressObserver *self )
{
  g_return_val_if_fail( ZYPP_PROGRESS_OBSERVER(self), nullptr );
  ZYPP_PROGRESS_OBSERVER_D();
  return d->_cppObj;
}
