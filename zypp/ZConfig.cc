/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ZConfig.cc
 *
*/
#include <iostream>
#include "zypp/base/Logger.h"
#include "zypp/base/InputStream.h"
#include "zypp/base/String.h"

#include "zypp/ZConfig.h"
#include "zypp/ZYppFactory.h"
#include "zypp/PathInfo.h"
#include "zypp/parser/IniDict.h"

using namespace std;
using namespace zypp::filesystem;
using namespace zypp::parser;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZConfig::Impl
  //
  /** ZConfig implementation.
   * \todo Enrich section and entry definition by some comment
   * (including the default setting and provide some method to
   * write this into a sample zypp.conf.
  */
  class ZConfig::Impl
  {
    public:
      Impl()
        : download_use_patchrpm( true )
        , download_use_deltarpm( true )
      {
        MIL << "ZConfig singleton created." << endl;

	// ZYPP_CONF might override /etc/zypp/zypp.conf
        const char *env_confpath = getenv( "ZYPP_CONF" );
        PathInfo confpath( env_confpath ? env_confpath : "/etc/zypp/zypp.conf" );

        if ( ! ( confpath.isFile() && confpath.isR() ) )
        {
          MIL << "Unable to read " << confpath << ": using defaults instead." << endl;
          return;
        }

        MIL << "Reading " << confpath << endl;
        parser::IniDict dict;
        dict.read( confpath.path() );

        for ( IniDict::section_const_iterator sit = dict.sectionsBegin();
              sit != dict.sectionsEnd();
              ++sit )
        {
          string section(*sit);
          //MIL << section << endl;
          for ( IniDict::entry_const_iterator it = dict.entriesBegin(*sit);
                it != dict.entriesEnd(*sit);
                ++it )
          {
            string entry(it->first);
            string value(it->second);
            //DBG << (*it).first << "=" << (*it).second << endl;
            if ( section == "main" )
            {
              if( entry == "download.use_patchrpm" )
              {
                download_use_patchrpm = str::strToBool( value, download_use_patchrpm );
              }
              else if ( entry == "download.use_deltarpm" )
              {
                download_use_deltarpm = str::strToBool( value, download_use_deltarpm );
              }
            }
          }
        }
      }

      ~Impl()
      {}

    public:
      bool download_use_patchrpm;
      bool download_use_deltarpm;
  };
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : ZConfig::instance
  //	METHOD TYPE : ZConfig &
  //
  ZConfig & ZConfig::instance()
  {
    static ZConfig _instance; // The singleton
    return _instance;
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : ZConfig::ZConfig
  //	METHOD TYPE : Ctor
  //
  ZConfig::ZConfig()
  : _pimpl( new Impl )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : ZConfig::~ZConfig
  //	METHOD TYPE : Dtor
  //
  ZConfig::~ZConfig( )
  {}

  bool ZConfig::download_use_patchrpm() const
  { return _pimpl->download_use_patchrpm; }

  bool ZConfig::download_use_deltarpm() const
  { return _pimpl->download_use_deltarpm; }


  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
