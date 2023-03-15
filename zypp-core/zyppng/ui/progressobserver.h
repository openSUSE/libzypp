/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_CORE_ZYPPNG_PROGRESSOBSERVER_H
#define ZYPP_CORE_ZYPPNG_PROGRESSOBSERVER_H

#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/base/Signals>
#include <zypp-core/zyppng/pipelines/Expected>
#include <optional>
#include <string>

// backwards compat
#include <zypp-core/ui/progressdata.h>


namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( ProgressObserver );
  class ProgressObserverPrivate;

  class ProgressObserver : public Base
  {
    ZYPP_DECLARE_PRIVATE(ProgressObserver)
    ZYPP_ADD_CREATE_FUNC(ProgressObserver)

  public:

    enum FinishResult {
      Success,
      Error
    };

    ZYPP_DECL_PRIVATE_CONSTR_ARGS(ProgressObserver, const std::string &label = std::string(), int steps = 100 );

    void    setBaseSteps( int steps );
    int     baseSteps   ( ) const;
    int     steps       ( ) const;
    bool    started     ( ) const;

    /*!
     * Tells the \ref ProgressObserver to start sending signals on updates.
     * If start was not called, no progress update or finished signals are emitted.
     *
     * Calling start will also trigger all parent observers to be started, but not the children
     */
    void    start       ( );
    void    reset       ( );
    void    setCurrent  ( double curr );
    void    setFinished ( FinishResult result = Success );
    void    inc         ( double inc = 1.0, const std::optional<std::string> &newLabel = {} );

    double  progress() const;
    double  current()  const;


    inline static ProgressObserverRef makeSubTask( ProgressObserverRef parentProgress, float weight = 1.0, const std::string &label = std::string(), int steps = 100  ) {
      if ( parentProgress ) return parentProgress->makeSubTask( weight, label, steps );
      return nullptr;
    }

    inline static void start ( ProgressObserverRef progress ) {
      if ( progress ) progress->start();
    }

    inline static void setup( ProgressObserverRef progress, const std::string &label = std::string(), int steps = 100  ) {
      if ( progress ) {
        progress->setLabel( label );
        progress->setBaseSteps ( steps );
      }
    }

    inline static void increase( ProgressObserverRef progress,  double inc = 1.0, const std::optional<std::string> &newLabel = {} ) {
      if ( progress ) progress->inc ( inc, newLabel );
    }

    inline static void setCurrent ( ProgressObserverRef progress, double curr ) {
      if ( progress ) progress->setCurrent ( curr );
    }

    inline static void setLabel ( ProgressObserverRef progress, const std::string &label ) {
      if ( progress ) progress->setLabel ( label );
    }

    inline static void setSteps ( ProgressObserverRef progress, int steps ) {
      if ( progress ) progress->setBaseSteps ( steps );
    }

    inline static void finish ( ProgressObserverRef progress, ProgressObserver::FinishResult result = ProgressObserver::Success ) {
      if ( progress ) progress->setFinished ( result );
    }

    const std::vector<zyppng::ProgressObserverRef> &children();

    void    setLabel    ( const std::string &label );
    const std::string &label () const;

    void registerSubTask ( const ProgressObserverRef& child, float weight = 1.0 );

    ProgressObserverRef makeSubTask( float weight = 1.0 ,const std::string &label = std::string(), int steps = 100 );

    zypp::ProgressData::ReceiverFnc makeProgressDataReceiver ();

    SignalProxy<void ( ProgressObserver &sender  )> sigStarted ();
    SignalProxy<void ( ProgressObserver &sender, const std::string &str )> sigLabelChanged ();
    SignalProxy<void ( ProgressObserver &sender, double steps )>    sigStepsChanged();
    SignalProxy<void ( ProgressObserver &sender, double current ) > sigValueChanged();
    SignalProxy<void ( ProgressObserver &sender, double progress )> sigProgressChanged();
    SignalProxy<void ( ProgressObserver &sender, FinishResult result )> sigFinished();
    SignalProxy<void ( ProgressObserver &sender, ProgressObserverRef child )> sigNewSubprogress();

  };

  namespace operators {

    namespace detail {

      enum class progress_helper_mode {
        Start,
        Increase,
        Set,
        Finish
      };

      template <progress_helper_mode mode>
      struct progress_helper {

        progress_helper( ProgressObserverRef &&progressObserver, std::optional<std::string> &&newStr, double inc )
          : _progressObserver( std::move(progressObserver) )
          , _newString( std::move(newStr) )
          , _progressInc( inc )
        {}

        template <typename T>
        auto operator() ( T &&t ) {
          update();
          return std::forward<T>(t);
        }

        void operator()() {
          update();
        }

        void update() {
          if ( _progressObserver ) {
            if constexpr ( mode == progress_helper_mode::Increase ) {
              _progressObserver->inc( _progressInc, _newString );
            } else {
              _progressObserver->setCurrent ( _progressInc );
              if ( _newString )
                _progressObserver->setLabel ( *_newString );
            }
          }
        }

      private:
        ProgressObserverRef _progressObserver;
        std::optional<std::string> _newString;
        double _progressInc;
      };

      template <>
      struct progress_helper<progress_helper_mode::Start>
      {
        progress_helper( ProgressObserverRef &&progressObserver )
          : _progressObserver( std::move(progressObserver) ){}

        template <typename T>
        auto operator() ( T &&t ) {
          update();
          return std::forward<T>(t);
        }

        void operator()() {
          update();
        }

        void update() {
          if ( _progressObserver ) { _progressObserver->start(); }
        }

        private:
          ProgressObserverRef _progressObserver;
      };

      template <>
      struct progress_helper<progress_helper_mode::Finish>
      {
        progress_helper( ProgressObserverRef &&progressObserver, ProgressObserver::FinishResult result = ProgressObserver::Success )
          : _progressObserver( std::move(progressObserver) )
          , _result(result){}

        template <typename T>
        auto operator() ( T &&t ) {
          update();
          return std::forward<T>(t);
        }

        void operator()() {
          update();
        }

        void update() {
          if ( _progressObserver ) { _progressObserver->setFinished( _result ); }
        }

        private:
          ProgressObserverRef _progressObserver;
          ProgressObserver::FinishResult _result;
      };
    }

    /*!
     * Starts the the given \ref ProgressObserver, forwarding the pipeline value without touching it
     */
    inline auto startProgress( ProgressObserverRef progressObserver ) {
      return detail::progress_helper<detail::progress_helper_mode::Start>( std::move(progressObserver) );
    }

    /*!
     * Increases the \ref ProgressObserver by the given \a progIncrease, forwarding the pipeline value without touching it.
     * Can also set a new label via \a newStr
     */
    inline auto incProgress( ProgressObserverRef progressObserver, double progrIncrease = 1.0, std::optional<std::string> newStr = {} ) {
      return detail::progress_helper<detail::progress_helper_mode::Increase>( std::move(progressObserver), std::move(newStr), progrIncrease );
    }

    /*!
     * Sets the current value in the given \ref ProgressObserver, forwarding the pipeline value without touching it
     * Can also set a new label via \a newStr
     */
    inline auto setProgress( ProgressObserverRef progressObserver, double progrValue, std::optional<std::string> newStr = {} ) {
      return detail::progress_helper<detail::progress_helper_mode::Set>( std::move(progressObserver), std::move(newStr), progrValue );
    }

    /*!
     * Sets the label value in the given \ref ProgressObserver, forwarding the pipeline value without touching it
     */
    inline auto setProgressLabel( ProgressObserverRef progressObserver, std::string newStr ) {
      // use the Increase functor, it allows us to let the progress value untouched and just sets the strings
      return detail::progress_helper<detail::progress_helper_mode::Increase>( std::move(progressObserver), std::move(newStr), 0.0 );
    }

    /*!
     * Sets the the given \ref ProgressObserver to finished, forwarding the pipeline value without touching it
     */
    inline auto finishProgress( ProgressObserverRef progressObserver, ProgressObserver::FinishResult result = ProgressObserver::Success ) {
      return detail::progress_helper<detail::progress_helper_mode::Finish>( std::move(progressObserver), result );
    }


  }

} // namespace zyppng

#endif // ZYPP_CORE_ZYPPNG_PROGRESSOBSERVER_H
