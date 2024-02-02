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
    ZYPP_DECL_PRIVATE_CONSTR_ARGS(ProgressObserver, const std::string &label = std::string(), int steps = 100 );

    void    setBaseSteps( int steps );
    int     baseSteps   ( ) const;
    int     steps       ( ) const;

    void    reset       ( );
    void    setCurrent  ( double curr );
    void    setFinished ( );
    void    inc         ( double inc = 1.0, const std::optional<std::string> &newLabel = {} );

    double  progress() const;
    double  current()  const;

    const std::vector<zyppng::ProgressObserverRef> &children();

    void    setLabel    ( const std::string &label );
    const std::string &label () const;

    void registerSubTask ( ProgressObserverRef child, float weight = 1.0 );

    zypp::ProgressData::ReceiverFnc makeProgressDataReceiver ();

    SignalProxy<void ( ProgressObserver &sender, const std::string &str )> sigLabelChanged ();
    SignalProxy<void ( ProgressObserver &sender, double steps )>    sigStepsChanged();
    SignalProxy<void ( ProgressObserver &sender, double current ) > sigValueChanged();
    SignalProxy<void ( ProgressObserver &sender, double progress )> sigProgressChanged();
    SignalProxy<void ( ProgressObserver &sender )> sigFinished();
    SignalProxy<void ( ProgressObserver &sender, ProgressObserverRef child )> sigNewSubprogress();

  };


  namespace operators {

    namespace detail {

      enum class progress_helper_mode {
        Increase,
        Set
      };

      template <auto mode = progress_helper_mode::Increase>
      struct progress_helper {

        progress_helper( ProgressObserverRef &&progressObserver, std::optional<std::string> &&newStr, double inc )
          : _progressObserver( progressObserver )
          , _newString( std::move(newStr) )
          , _progressInc( inc )
        {}

        template <typename T>
        void operator() ( T && ) {
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
    inline auto setProgressLabel( ProgressObserverRef progressObserver, std::string &&newStr ) {
      // use the Increase functor, it allows us to let the progress value untouched and just sets the strings
      return detail::progress_helper<detail::progress_helper_mode::Increase>( std::move(progressObserver), std::move(newStr), 0.0 );
    }

    /*!
     * Sets the the given \ref ProgressObserver to finished, forwarding the pipeline value without touching it
     */
    inline auto finishProgress( ProgressObserverRef progressObserver ) {
      return [ progressObserver = std::move(progressObserver) ]( auto &&val ) {
        if ( progressObserver ) progressObserver->setFinished ();
        return std::forward<decltype(val)>(val);
      };
    }


  }

} // namespace zyppng

#endif // ZYPPNG_PROGRESSOBSERVER_H
