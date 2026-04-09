/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// Automatically generated wrapper for zypp/base/SetTracker.h
#ifndef SETTRACKER_H_ZYPPNG_WRAPPER_H
#define SETTRACKER_H_ZYPPNG_WRAPPER_H

#include <zypp/base/SetTracker.h>

namespace zyppng
{

  namespace base {
    using ::zypp::base::SetTracker;

    // ADL specific wrapper
    template <typename TSet>
    inline std::ostream & operator<<(std::ostream & str, const SetTracker<TSet> & obj) { return ::zypp::base::operator<<(str, obj); }
  } // namespace base

} // namespace zyppng
#endif // SETTRACKER_H_ZYPPNG_WRAPPER_H
