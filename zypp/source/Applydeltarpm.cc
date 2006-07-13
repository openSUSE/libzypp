/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/source/Applydeltarpm.cc
 *
*/
#include <iostream>
#include "zypp/base/Logger.h"

#include "zypp/source/Applydeltarpm.h"
#include "zypp/ExternalProgram.h"
#include "zypp/PathInfo.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace applydeltarpm
  { /////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////
    namespace
    { /////////////////////////////////////////////////////////////////

      const Pathname applydeltarpm_prog( "/usr/bin/applydeltarpm" );

      /////////////////////////////////////////////////////////////////
    } // namespace
    ///////////////////////////////////////////////////////////////////

    bool haveApplydeltarpm()
    {
      static bool _last = false;

      PathInfo prog( applydeltarpm_prog );
      if ( _last !=  prog.isX() )
        {
          if ( (_last = !_last) )
            MIL << "Found executable " << prog << endl;
          else
            WAR << "No executable " << prog << endl;
        }
      return _last;
    }

    bool check( const std::string & sequenceinfo_r, bool quick_r )
    {
      return false;
    }

    bool provide( const Pathname & delta_r, const Pathname & new_r )
    {
      return false;
    }

    /////////////////////////////////////////////////////////////////
  } // namespace applydeltarpm
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
