/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPPNG_PROGRESSOBSERVER_H
#define ZYPPNG_PROGRESSOBSERVER_H

#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/base/Signals>
#include <optional>


namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( ProgressObserver );
  class ProgressObserverPrivate;

  class ProgressObserver : public Base
  {
    ZYPP_DECLARE_PRIVATE(ProgressObserver)
    ZYPP_ADD_CREATE_FUNC(ProgressObserver)

  public:
    ZYPP_DECL_PRIVATE_CONSTR_ARGS(ProgressObserver, const std::string &label = std::string(), int steps = 100 );

    void    setBaseSteps( int steps );
    int     baseSteps   ( ) const;
    int     steps       ( ) const;

    void    reset       ( );
    void    setCurrent  ( double curr );
    void    setFinished ( );
    void    inc         ( double inc, const std::optional<std::string> &newLabel );

    double  progress() const;
    double  current()  const;

    const std::vector<zyppng::ProgressObserverRef> &children();

    void    setLabel    ( const std::string &label );
    const std::string &label () const;

    void registerSubTask ( ProgressObserverRef child, float weight = 1.0 );

    SignalProxy<void ( ProgressObserver &sender, const std::string &str )> sigLabelChanged ();
    SignalProxy<void ( ProgressObserver &sender, double steps )>    sigStepsChanged();
    SignalProxy<void ( ProgressObserver &sender, double current ) > sigValueChanged();
    SignalProxy<void ( ProgressObserver &sender, double progress )> sigProgressChanged();
    SignalProxy<void ( ProgressObserver &sender )> sigFinished();
    SignalProxy<void ( ProgressObserver &sender, ProgressObserverRef child )> sigNewSubprogress();

  };

} // namespace zyppng

#endif // ZYPPNG_PROGRESSOBSERVER_H
