/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/progressobserver_p.h"
#include <new>

#include <zypp-core/base/LogControl.h>
#include <zypp-core/base/String.h>
#include <iostream>

G_DEFINE_FINAL_TYPE_WITH_PRIVATE( ZyppProgressObserver, zypp_progress_observer, G_TYPE_OBJECT )

/* Signals */
typedef enum {
  SIG_FINISHED = 1,
  SIG_NEW_CHILD,
  LAST_SIGNAL
} ZyppProgressObserverSignals;

static guint signals[LAST_SIGNAL] = { 0, };

typedef enum
{
  PROP_LABEL = 1,
  PROP_BASE_STEPS,
  PROP_STEPS,
  PROP_VALUE,
  PROP_PROGRESS,
  N_PROPERTIES
} ZyppProgressObserverProperty;


static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void zypp_progress_observer_finalize     (GObject *gobject);
static void zypp_progress_observer_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void zypp_progress_observer_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

static void zypp_progress_observer_class_init (ZyppProgressObserverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize     = zypp_progress_observer_finalize;
  object_class->get_property = zypp_progress_observer_get_property;
  object_class->set_property = zypp_progress_observer_set_property;

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

static void child_finished ( ZyppProgressObserver *sender, ZyppProgressObserver *self )
{
  //forget about it
  g_signal_handlers_disconnect_by_data( sender, self );
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));

  auto i =  p->_children.find( sender );
  if ( i != p->_children.end() )
    p->_children.remove(i);
}

// here in the C world we only need to track the childs lifetime, progress calculation
// and tracking is done in the C++ world anyway
static void zypp_progress_observer_reg_child ( ZyppProgressObserver *self, zyppng::ZyppProgressObserverRef &&child )
{
  if ( !child )
    return;

  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));

  if ( p->_children.find( child.get() )!= p->_children.end() )
    return;

  // first things first: make sure the ref is in our list so its cleaned up
  p->_children.push_back ( child.get() );

  // the ref is now tracked by the list
  auto rawChild = child.detach();

  // once the child is finished, forget about it
  g_signal_connect ( rawChild, "finished", G_CALLBACK( child_finished ), self );
}

static void
zypp_progress_observer_set_property (GObject      *object,
  guint         property_id,
  const GValue *value,
  GParamSpec   *pspec)
{
  g_return_if_fail( ZYPP_IS_PROGRESS_OBSERVER(object) );
  ZyppProgressObserver *self = ZYPP_PROGRESS_OBSERVER (object);

  switch ((ZyppProgressObserverProperty)property_id ) {
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
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));

  switch ((ZyppProgressObserverProperty)property_id )
  {
    case PROP_LABEL: {
      g_value_set_string ( value, p->_wrappedObj->label().c_str() );
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


static void zypp_progress_observer_init ( ZyppProgressObserver *self )
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  new( p ) ZyppProgressObserverPrivate( self ); // call all constructors
  p->initialize ();
}

static void zypp_progress_observer_finalize (GObject *gobject)
{
  ZyppProgressObserver *self = ZYPP_PROGRESS_OBSERVER(gobject);
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));

  // make sure our children don't try to send data to us anymore.
  for ( ZyppProgressObserver *child : p->_children )
    g_signal_handlers_disconnect_by_data( child, self );

  p->~ZyppProgressObserverPrivate();

  // always call parent class finalizer
  G_OBJECT_CLASS (zypp_progress_observer_parent_class)->finalize (gobject);
}


void zypp_progress_observer_add_subtask( ZyppProgressObserver *self, ZyppProgressObserver *newChild, gfloat weight )
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  auto pChild = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( newChild ));
  p->_wrappedObj->registerSubTask ( pChild->_wrappedObj, weight );
}

const gchar *zypp_progress_observer_get_label ( ZyppProgressObserver *self )
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  return p->_wrappedObj->label().c_str();
}

gdouble zypp_progress_observer_get_steps(ZyppProgressObserver *self)
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  return p->_wrappedObj->steps();
}

gint zypp_progress_observer_get_base_steps(ZyppProgressObserver *self)
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  return p->_wrappedObj->baseSteps();
}

gdouble zypp_progress_observer_get_current  ( ZyppProgressObserver *self )
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  return p->_wrappedObj->current();
}

gdouble zypp_progress_observer_get_progress  ( ZyppProgressObserver *self )
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  return p->_wrappedObj->progress ();
}

void zypp_progress_observer_set_label(ZyppProgressObserver *self, const gchar *label)
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  p->_wrappedObj->setLabel( zypp::str::asString(label) );
}

void zypp_progress_observer_set_base_steps (ZyppProgressObserver *self, gint max )
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  p->_wrappedObj->setBaseSteps(max);
}

void zypp_progress_observer_set_current  (ZyppProgressObserver *self, gdouble value )
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  p->_wrappedObj->setCurrent(value);
}

void zypp_progress_observer_inc(ZyppProgressObserver *self, gint increase)
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  p->_wrappedObj->inc(increase, {});
}

void zypp_progress_observer_set_finished(ZyppProgressObserver *self)
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  p->_wrappedObj->setFinished();
}

const GList *zypp_progress_observer_get_children(ZyppProgressObserver *self)
{
  auto p = static_cast<ZyppProgressObserverPrivate *>(zypp_progress_observer_get_instance_private( self ));
  return p->_children.get();
}

void ZyppProgressObserverPrivate::assignCppType(zyppng::ProgressObserverRef &&ptr) {
  WrapperPrivateBase::assignCppType( std::move(ptr) );

  _signalConns.insert ( _signalConns.end(), {
     _wrappedObj->connectFunc( &zyppng::ProgressObserver::sigLabelChanged, [this]( auto &, const std::string & ){
       g_object_notify_by_pspec ( G_OBJECT(_gObject), obj_properties[PROP_LABEL] );
     }),
     _wrappedObj->connectFunc( &zyppng::ProgressObserver::sigStepsChanged, [this]( auto &, double ){
       g_object_notify_by_pspec ( G_OBJECT(_gObject), obj_properties[PROP_STEPS] );
     }),
     _wrappedObj->connectFunc( &zyppng::ProgressObserver::sigValueChanged, [this]( auto &, double ){
       g_object_notify_by_pspec ( G_OBJECT(_gObject), obj_properties[PROP_VALUE] );
     }),
     _wrappedObj->connectFunc( &zyppng::ProgressObserver::sigProgressChanged, [this]( auto &, double ){
       g_object_notify_by_pspec ( G_OBJECT(_gObject), obj_properties[PROP_PROGRESS] );
     }),
     _wrappedObj->connectFunc( &zyppng::ProgressObserver::sigFinished, [this]( zyppng::ProgressObserver &sender ){
       g_signal_emit (_gObject, signals[SIG_FINISHED], 0);
     }),
     _wrappedObj->connectFunc( &zyppng::ProgressObserver::sigNewSubprogress, [this]( auto &, zyppng::ProgressObserverRef newRef ){
       auto wrapped = zyppng::zypp_wrap_cpp<ZyppProgressObserver>( newRef );
       zypp_progress_observer_reg_child ( _gObject, zyppng::ZyppProgressObserverRef(wrapped) );
       g_signal_emit( _gObject, signals[SIG_NEW_CHILD], 0, wrapped.get() );
     })
   });

  for ( auto chld : _wrappedObj->children () ) {
    zypp_progress_observer_reg_child( _gObject, zyppng::zypp_wrap_cpp<ZyppProgressObserver>(chld) );
  }
}
