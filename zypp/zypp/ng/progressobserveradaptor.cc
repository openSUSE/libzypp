/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "progressobserveradaptor.h"

namespace zyppng {

  void ProgressObserverAdaptor::init()
  {
    _observer = ProgressObserver::create();
    _observer->connectFunc(
          &ProgressObserver::sigStarted,
          [this](auto &obs) { update(); }, *_observer);
    _observer->connectFunc(
          &ProgressObserver::sigLabelChanged,
          [this](auto &obs, const auto &) { if(!_first) update(); }, *_observer);
    _observer->connectFunc(
          &ProgressObserver::sigProgressChanged,
          [this](auto &obs, const auto &p) { update(); }, *_observer);
    _observer->connectFunc(
          &ProgressObserver::sigFinished,
          [this](auto &obs, const auto &) { update(true); }, *_observer);
  }

  ProgressObserverAdaptor::ProgressObserverAdaptor(
      zypp::callback::SendReport<zypp::ProgressReport> &report)
    : _report(report)
  {
    init();
  }
  ProgressObserverAdaptor::ProgressObserverAdaptor(
      const zypp::ProgressData::ReceiverFnc &fnc,
      zypp::callback::SendReport<zypp::ProgressReport> &report)
    : _fnc(fnc), _report(report)
  {
    init();
  }
  ProgressObserverRef ProgressObserverAdaptor::observer() { return _observer; }
  bool ProgressObserverAdaptor::update(bool fin) {
    zypp::ProgressData progress(0, 100, _observer->progress());
    progress.name(_observer->label());

    if (_first) {
      _report->start(progress);
      _first = false;
    }

    bool value = _report->progress(progress);
    if (_fnc)
      value &= _fnc(progress);

    if (fin) {
      _report->finish(progress);
    }
    return value;
  }
} // namespace zyppng
