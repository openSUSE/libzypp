/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "OptionalDownloadProgressReport.h"

namespace internal {

  using namespace zypp;

  OptionalDownloadProgressReport::OptionalDownloadProgressReport(bool isOptional)
    : _oldRec { Distributor::instance().getReceiver() }
    , _isOptional { isOptional }
  { connect(); }

  OptionalDownloadProgressReport::~OptionalDownloadProgressReport() {
    if (_oldRec)
      Distributor::instance().setReceiver(*_oldRec);
    else
      Distributor::instance().noReceiver();
  }

  void OptionalDownloadProgressReport::reportbegin()
  { if ( _oldRec ) _oldRec->reportbegin(); }

  void OptionalDownloadProgressReport::reportend()
  { if ( _oldRec ) _oldRec->reportend(); }

  void OptionalDownloadProgressReport::report(const UserData &userData_r)
  { if ( _oldRec ) _oldRec->report( userData_r ); }

  void OptionalDownloadProgressReport::start(const Url &file_r, Pathname localfile_r)
  {
    if ( not _oldRec ) return;
    if ( _isOptional ) {
      // delay start until first data are received.
      _startFile      = file_r;
      _startLocalfile = std::move(localfile_r);
      return;
    }
    _oldRec->start( file_r, localfile_r );
  }

  bool OptionalDownloadProgressReport::progress(int value_r, const Url &file_r, double dbps_avg_r, double dbps_current_r)
  {
    if ( not _oldRec ) return true;
    if ( notStarted() ) {
      if ( not ( value_r || dbps_avg_r || dbps_current_r ) )
        return true;
      sendStart();
    }

    //static constexpr std::chrono::milliseconds minfequency { 1000 }; only needed if we'd avoid sending reports without change
    static constexpr std::chrono::milliseconds maxfequency { 100 };
    TimePoint now { TimePoint::clock::now() };
    TimePoint::duration elapsed { now - _lastProgressSent };
    if ( elapsed < maxfequency )
      return true;  // continue
    _lastProgressSent = now;
    return _oldRec->progress( value_r, file_r, dbps_avg_r, dbps_current_r );
  }

  media::DownloadProgressReport::Action OptionalDownloadProgressReport::problem(const Url &file_r, Error error_r, const std::string &description_r)
  {
    if ( not _oldRec || notStarted() ) return ABORT;
    return _oldRec->problem( file_r, error_r, description_r );
  }

  void OptionalDownloadProgressReport::finish(const Url &file_r, Error error_r, const std::string &reason_r)
  {
    if ( not _oldRec || notStarted() ) return;
    _oldRec->finish( file_r, error_r, reason_r );
  }

  bool OptionalDownloadProgressReport::notStarted() const
  { return _isOptional; }

  void OptionalDownloadProgressReport::sendStart()
  {
    if ( _isOptional ) {
      // we know _oldRec is valid...
      _oldRec->start( _startFile, std::move(_startLocalfile) );
      _isOptional = false;
    }
  }

}
