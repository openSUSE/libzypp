/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "DownloadProgressTracker.h"

#include <algorithm>

namespace zypp::internal {

void ProgressTracker::updateStats(double dltotal, double dlnow)
{
  clock::time_point now = clock::now();

  if ( !_timeStart )
    _timeStart = _timeLast = now;

  // If called without args (0.0), recompute based on the last values seen
  if ( dltotal && dltotal != _dnlTotal )
    _dnlTotal = dltotal;

  if ( dlnow && dlnow != _dnlNow ) {
    _dnlNow = dlnow;
  }

  // percentage:
  if ( _dnlTotal )
    _dnlPercent = int(_dnlNow * 100 / _dnlTotal);

  // download rates:
  _drateTotal = _dnlNow / std::max( std::chrono::duration_cast<std::chrono::seconds>(now - *_timeStart).count(), int64_t(1) );

  if ( _timeLast < now )
  {
    const auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - *_timeLast).count();
    if ( duration >= 1 ) {
      _drateLast = (_dnlNow - _dnlLast) / duration;
      // start new period
      _timeLast  = now;
      _dnlLast   = _dnlNow;
    }
  }
  else if ( _timeStart == _timeLast )
    _drateLast = _drateTotal;
}

}
