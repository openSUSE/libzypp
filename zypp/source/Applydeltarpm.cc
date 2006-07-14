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
#include "zypp/AutoDispose.h"
#include "zypp/PathInfo.h"
#include "zypp/TriBool.h"

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

    /******************************************************************
     **
     **	FUNCTION NAME : haveApplydeltarpm
     **	FUNCTION TYPE : bool
    */
    bool haveApplydeltarpm()
    {
      // To track changes in availability of applydeltarpm.
      static TriBool _last = indeterminate;
      PathInfo prog( applydeltarpm_prog );
      bool have = prog.isX();
      if ( _last == have )
        ; // TriBool! 'else' is not '_last != have'
      else
        {
          // _last is 'indeterminate' or '!have'
          if ( (_last = have) )
            MIL << "Found executable " << prog << endl;
          else
            WAR << "No executable " << prog << endl;
        }
      return _last;
    }

    /******************************************************************
     **
     **	FUNCTION NAME : check
     **	FUNCTION TYPE : bool
    */
    bool check( const std::string & sequenceinfo_r, bool quick_r )
    {
      if ( ! haveApplydeltarpm() )
        return false;

      if ( sequenceinfo_r.empty() )
        {
          DBG << "Applydeltarpm " << (quick_r?"quickcheck":"check") << " -> empty sequenceinfo" << endl;
          return false;
        }

      const char* argv[] = {
        "/usr/bin/applydeltarpm",
        ( quick_r ? "-C" : "-c" ),
        "-s", sequenceinfo_r.c_str(),
        NULL
      };

      ExternalProgram prog( argv, ExternalProgram::Stderr_To_Stdout );
      for ( std::string line = prog.receiveLine(); ! line.empty(); line = prog.receiveLine() )
        {
          DBG << "Applydeltarpm " << (quick_r?"quickcheck":"check") << ": " << line;
        }
      int exit_code = prog.close();

      DBG << "Applydeltarpm " << (quick_r?"quickcheck":"check") << " -> " << exit_code << endl;
      return( exit_code == 0 );
    }

    /******************************************************************
     **
     **	FUNCTION NAME : provide
     **	FUNCTION TYPE : bool
    */
    bool provide( const Pathname & delta_r, const Pathname & new_r )
    {
      // cleanup on error
      AutoDispose<const Pathname> guard( new_r, filesystem::unlink );

      if ( ! haveApplydeltarpm() )
        return false;

      const char* argv[] = {
        "/usr/bin/applydeltarpm",
        "-p",
        "-v",
        delta_r.asString().c_str(),
        new_r.asString().c_str(),
        NULL
      };

      ExternalProgram prog( argv, ExternalProgram::Stderr_To_Stdout );
      for ( std::string line = prog.receiveLine(); ! line.empty(); line = prog.receiveLine() )
        {
          DBG << "Applydeltarpm: " << line;
        }

      int exit_code = prog.close();
      DBG << "Applydeltarpm -> " << exit_code << endl;
      if ( exit_code != 0 )
        return false;


      guard.resetDispose(); // no cleanup on success
      return true;
    }

    /////////////////////////////////////////////////////////////////
  } // namespace applydeltarpm
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
