/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_MEDIA_DETAIL_DOWNLOADPROGRESSTRACKER_INCLUDED
#define ZYPP_MEDIA_DETAIL_DOWNLOADPROGRESSTRACKER_INCLUDED

#include <chrono>
#include <optional>

namespace zypp::internal {

struct ProgressTracker {

  using clock = std::chrono::steady_clock;

  std::optional<clock::time_point> _timeStart; ///< Start total stats
  std::optional<clock::time_point> _timeLast;	 ///< Start last period(~1sec)

  double _dnlTotal	= 0.0;	///< Bytes to download or 0 if unknown
  double _dnlLast	= 0.0;	  ///< Bytes downloaded at period start
  double _dnlNow	= 0.0;	  ///< Bytes downloaded now

  int    _dnlPercent= 0;	///< Percent completed or 0 if _dnlTotal is unknown

  double _drateTotal= 0.0;	///< Download rate so far
  double _drateLast	= 0.0;	///< Download rate in last period

  void updateStats( double dltotal = 0.0, double dlnow = 0.0 );
};


}


#endif
