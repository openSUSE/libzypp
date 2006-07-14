/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/source/Applydeltarpm.h
 *
*/
#ifndef ZYPP_SOURCE_APPLYDELTARPM_H
#define ZYPP_SOURCE_APPLYDELTARPM_H

#include <iosfwd>
#include <string>

#include "zypp/base/PtrTypes.h"
#include "zypp/Pathname.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  /** Namespace wrapping invocations of /usr/bin/applydeltarpm. */
  ///////////////////////////////////////////////////////////////////
  namespace applydeltarpm
  { /////////////////////////////////////////////////////////////////

    /** Test whether an execuatble applydeltarpm program is available. */
    bool haveApplydeltarpm();

    /** Check if reconstruction of rpm is possible.
     * \see <tt>man applydeltarpm ([-c|-C] -s sequence)<\tt>
    */
    bool check( const std::string & sequenceinfo_r, bool quick_r = false );

    /** \see check */
    inline bool quickcheck( const std::string & sequenceinfo_r )
    { return check( sequenceinfo_r, true ); }

    /** Apply a binary delta to on-disk data to re-create a new rpm.
     * \see <tt>man applydeltarpm (deltarpm newrpm)<\tt>
    */
    bool provide( const Pathname & delta_r, const Pathname & new_r );

    /////////////////////////////////////////////////////////////////
  } // namespace applydeltarpm
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SOURCE_APPLYDELTARPM_H
