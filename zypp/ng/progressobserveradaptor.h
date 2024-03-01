/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_PROGRESSOBSERVERADAPTOR_INCLUDED
#define ZYPP_NG_PROGRESSOBSERVERADAPTOR_INCLUDED

#include <zypp/ZYppCallbacks.h>
#include <zypp-core/zyppng/ui/ProgressObserver>

namespace zyppng {
  struct ProgressObserverAdaptor
  {

    ProgressObserverAdaptor( zypp::callback::SendReport<zypp::ProgressReport> &report );

    ProgressObserverAdaptor( const zypp::ProgressData::ReceiverFnc &fnc, zypp::callback::SendReport<zypp::ProgressReport> &report );

    ProgressObserverRef observer();

  private:
    void init();
    bool update(bool fin = false);

    ProgressObserverRef _observer;
    zypp::ProgressData::ReceiverFnc _fnc;
    zypp::callback::SendReport<zypp::ProgressReport> &_report;
    bool _first = true;
  };
}

#endif // ZYPP_NG_PROGRESSOBSERVERADAPTOR_INCLUDED
