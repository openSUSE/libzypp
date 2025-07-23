#include "progressobserver.h"
#include "zypp-core/AutoDispose.h"
#include <zypp-core/zyppng/base/private/base_p.h>
#include <zypp-core/base/dtorreset.h>

namespace zyppng {

  class ProgressObserverPrivate : public BasePrivate
  {
    ZYPP_DECLARE_PUBLIC(ProgressObserver)
  public:
    ProgressObserverPrivate( ProgressObserver &p ) : BasePrivate(p) {}


    void updateProgress ();
    void setLabel( const std::string &label );


    ProgressObserverWeakRef _parent;
    std::string _label;

    // the current counter value
    double _counterValue   = 0;

    // the number of steps we are going to have
    double _counterSteps = 0;

    // the value of all finished progresses
    double _finishedValue  = 0;

    // the steps of all finished progresses
    double _finishedSteps  = 0;

    int _baseValue = 0;
    int _baseSteps = 100;

    bool _started         = false;
    bool _ignoreChildSigs = false;

    struct ChildInfo {
      ChildInfo( std::vector<connection> &&conns, float weight ) : _signalConns( std::move(conns) ), _childWeight( weight ) {}
      ChildInfo( ChildInfo &&other ) noexcept
        : _signalConns( std::move(other._signalConns) )
        , _childWeight( other._childWeight )
      {
        other._signalConns.clear ();
      }
      ChildInfo( const ChildInfo &other ) = delete;
      ~ChildInfo() {
        std::for_each( _signalConns.begin (), _signalConns.end(), []( auto &sig ){ sig.disconnect(); });
      }

      ChildInfo &operator=( const ChildInfo & ) = delete;
      ChildInfo &operator=( ChildInfo &&other )
      {
        _childWeight = other._childWeight;
        _signalConns = std::move(other._signalConns);
        other._signalConns.clear ();
        return *this;
      }

      std::vector<connection> _signalConns;
      float _childWeight = 1.0;  // the factor how much increase a step in the child is worth in the parent
    };

    std::vector<ProgressObserverRef> _children;
    std::vector< ChildInfo > _childInfo;

    void onChildStarted(ProgressObserver &);
    void updateCounters();
    void onChildFinished( ProgressObserver &child , ProgressObserver::FinishResult );

    Signal<void ( ProgressObserver &sender) >                             _sigStarted;
    Signal<void ( ProgressObserver &sender, const std::string &str )>     _sigLabelChanged;
    Signal<void ( ProgressObserver &sender, double steps ) >              _sigStepsChanged;
    Signal<void ( ProgressObserver &sender, double steps ) >              _sigValueChanged;
    Signal<void ( ProgressObserver &sender, double steps ) >              _sigProgressChanged;
    Signal<void ( ProgressObserver &sender, ProgressObserver::FinishResult )>  _sigFinished;
    Signal<void ( ProgressObserver &sender, ProgressObserverRef child )>  _sigNewSubprogress;
  };

  ZYPP_IMPL_PRIVATE(ProgressObserver)

  void ProgressObserverPrivate::onChildStarted( ProgressObserver &/*child*/ )
  {
    if ( !_started ) z_func()->start();
  }

  void ProgressObserverPrivate::updateCounters()
  {
    if ( _ignoreChildSigs )
      return;

    double currProgressSteps = _baseValue;
    double accumSteps        = static_cast<double>(_baseSteps);

    for ( std::vector<ProgressObserverRef>::size_type i = 0; i < _children.size (); i++ ) {
      const auto childPtr   = _children[i].get();
      const auto &childInfo = _childInfo[i];
      const auto weight = childInfo._childWeight;
      currProgressSteps +=  childPtr->current() * weight;
      accumSteps += childPtr->steps()* weight;
    }

    bool notifyAccuMaxSteps   = _counterSteps != accumSteps ;
    bool notifyCurrSteps      = _counterValue != currProgressSteps;

    _counterSteps = accumSteps;
    _counterValue = currProgressSteps;

    if ( !_started )
      return;

    if ( notifyAccuMaxSteps )
      _sigStepsChanged.emit( *z_func(), _counterSteps );
    if ( notifyCurrSteps) {
      _sigValueChanged.emit( *z_func(), notifyCurrSteps );
      _sigProgressChanged.emit( *z_func(), z_func()->progress() );
    }
  }

  void ProgressObserverPrivate::onChildFinished(ProgressObserver &child, ProgressObserver::FinishResult )
  {
    auto i = std::find_if( _children.begin (), _children.end (), [&]( const auto &elem ) { return ( &child == elem.get() ); } );
    if ( i == _children.end() ) {
      WAR << "Unknown child sent a finished message, ignoring" << std::endl;
      return;
    }

    const auto idx = std::distance ( _children.begin (), i );
    _children.erase(i);
    _childInfo.erase( _childInfo.begin () + idx );
    updateCounters();
  }

  ZYPP_IMPL_PRIVATE_CONSTR_ARGS ( ProgressObserver, const std::string &label, int steps ) : Base( *( new ProgressObserverPrivate( *this ) ) )
  {
    Z_D();
    d->_baseSteps = steps;
    d->_label     = label;
    d->updateCounters();
  }

  void ProgressObserverPrivate::setLabel(const std::string &label)
  {
    if ( _label == label )
      return;
    _label = label;
    _sigLabelChanged.emit ( *z_func(), label );
  }


  int ProgressObserver::steps() const
  {
    return d_func()->_counterSteps;
  }

  bool ProgressObserver::started() const
  {
    return d_func()->_started;
  }

  void ProgressObserver::start()
  {
    Z_D();
    if ( d->_started )
      return;

    d->updateCounters();

    d->_started = true;
    d->_sigStarted.emit( *this );
  }

  void ProgressObserver::reset()
  {
    Z_D();
    {
      zypp::DtorReset deferedReset( d->_ignoreChildSigs, false );
      d->_ignoreChildSigs = true;
      std::for_each( d->_children.begin (), d->_children.end(), []( auto &child ) { child->reset(); });
    }
    d->updateCounters();
  }

  double ProgressObserver::progress() const
  {
    return 100.0 * current() / steps();
  }

  double ProgressObserver::current() const
  {
    return d_func()->_counterValue;
  }

  const std::vector<ProgressObserverRef> &ProgressObserver::children()
  {
    return d_func()->_children;
  }

  const std::string &ProgressObserver::label() const
  {
    return d_func()->_label;
  }

  SignalProxy<void (ProgressObserver &)> ProgressObserver::sigStarted()
  {
    return d_func()->_sigStarted;
  }

  SignalProxy<void (ProgressObserver &, const std::string &)> ProgressObserver::sigLabelChanged()
  {
    return d_func()->_sigLabelChanged;
  }

  SignalProxy<void (ProgressObserver &, double)> ProgressObserver::sigStepsChanged()
  {
    return d_func()->_sigStepsChanged;
  }

  SignalProxy<void (ProgressObserver &, double steps)> ProgressObserver::sigValueChanged()
  {
    return d_func()->_sigValueChanged;
  }

  SignalProxy<void (ProgressObserver &, double)> ProgressObserver::sigProgressChanged()
  {
    return d_func()->_sigProgressChanged;
  }

  SignalProxy<void (ProgressObserver &sender, ProgressObserver::FinishResult result)> ProgressObserver::sigFinished()
  {
    return d_func()->_sigFinished;
  }

  SignalProxy<void (ProgressObserver &, ProgressObserverRef)> ProgressObserver::sigNewSubprogress()
  {
    return d_func()->_sigNewSubprogress;
  }

  void ProgressObserver::setBaseSteps(int steps)
  {
    Z_D();
    if ( d->_baseSteps == steps )
      return;

    d->_baseSteps = steps;

    // update stats
    d->updateCounters();
  }

  void ProgressObserver::setLabel(const std::string &label)
  {
    d_func()->setLabel( label );
  }

  void ProgressObserver::setCurrent(double curr)
  {
    Z_D();
    auto set = std::max<double>(0, std::min<double>( curr, d->_baseSteps ) );
    if ( set == d->_baseValue )
      return;
    d->_baseValue = set;

    //update stats
    d->updateCounters();
  }

  int ProgressObserver::baseSteps() const
  {
    return d_func()->_baseSteps;
  }

  void ProgressObserver::setFinished( FinishResult result )
  {
    Z_D();

    // finish all children first, started children are removed via their finished signal
    // others we have to manually remove
    while ( d->_children.size() ) {
      auto back   = d->_children.back();
      bool remove = !back->started ();
      back->setFinished( result );
      if ( remove ) d->_children.pop_back();
    }

    if ( result != Error )
      setCurrent( d->_baseSteps );

    if ( d->_started )
      d->_sigFinished.emit( *this , result );
  }

  void ProgressObserver::inc( double inc, const std::optional<std::string> &newLabel )
  {
    setCurrent ( d_func()->_baseValue + inc );
    if ( newLabel ) setLabel ( *newLabel );
  }

  void ProgressObserver::registerSubTask( const ProgressObserverRef& child, float weight )
  {
    Z_D();
    auto i = std::find( d->_children.begin(), d->_children.end(), child );
    const auto adjustedWeight = std::min<float>( std::max<float>( 0.0, weight ), 1.0 );
    if ( i != d->_children.end() ) {
      const auto index = std::distance ( d->_children.begin (), i );
      d->_childInfo[index]._childWeight = adjustedWeight;
    } else {
      d->_children.push_back( child );
      d->_childInfo.push_back( {
        { connectFunc ( *child, &ProgressObserver::sigStepsChanged, [this]( auto &sender, auto ){ d_func()->updateCounters(); }, *this ),
          connectFunc ( *child, &ProgressObserver::sigValueChanged, [this]( auto &sender, auto ){ d_func()->updateCounters(); }, *this ),
          connect     ( *child, &ProgressObserver::sigStarted, *d, &ProgressObserverPrivate::onChildStarted ),
          connect     ( *child, &ProgressObserver::sigFinished, *d, &ProgressObserverPrivate::onChildFinished )
        }
        , adjustedWeight
      });
      d->_sigNewSubprogress.emit( *this, child );

      // if the child has been started already, we also need to start()
      if ( child->started () )
        start();
    }

    // update stats
    d->updateCounters();
  }

  ProgressObserverRef ProgressObserver::makeSubTask( float weight, const std::string &label, int steps )
  {
    auto r = ProgressObserver::create( label, steps );
    registerSubTask ( r, weight );
    return r;
  }

  /*!
   * Creates a new \ref zypp::ProgressData::ReceiverFnc and links it to the current ProgressObserver,
   * this can be used to interface with zypp code that was not yet migrated to the new ProgressObserver
   * API.
   *
   * \note the returned \ref zypp::ProgressData::ReceiverFnc will keep a reference to the \a ProgressObserver
   */
  zypp::ProgressData::ReceiverFnc ProgressObserver::makeProgressDataReceiver()
  {
    return [ sThis = shared_this<ProgressObserver>() ](  const zypp::ProgressData & data ){
      auto instance = sThis.get();
      instance->start();
      instance->setBaseSteps ( data.max () - data.min () );
      instance->setCurrent ( data.val () - data.min () );
      instance->setLabel ( data.name () );
      if ( data.finalReport() )
        instance->setFinished();
      return true;
    };
  }

} // namespace zyppng
