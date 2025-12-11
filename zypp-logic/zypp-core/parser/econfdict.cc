/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp-core/parser/econfdict.cc
*/
#include "econfdict.h"

#include <optional>

#include <zypp-core/Pathname.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/base/Exception.h>
#include <zypp-core/base/LogTools.h>

namespace zypp {

  namespace econf {

    class EconfException : public Exception
    {
    public:
      EconfException()
      : Exception( "Econf Exception" )
      {}
      EconfException( std::string msg_r )
      : Exception( std::move(msg_r) )
      {}
    };

    /**
     * Provide access to the prioritized list of files and drop-ins
     * to read and merge for a specific config file. The list is ordered
     * from lowest priority to highest priority,
     *
     * The rules are defined by the UAPI.6 Configuration Files
     * Specification (version 1)[1], but may be changed to follow
     * newer versions in the future.
     *
     * \see [1] https://github.com/uapi-group/specifications/blob/main/specs/configuration_files_specification.md
     */
    struct ConfigurationContext
    {
      using Exception = EconfException;

      ConfigurationContext()
      : ConfigurationContext( "/etc", "/usr/etc" )
      {}

      ConfigurationContext( std::optional<Pathname> etcDir_r, std::optional<Pathname> usrDir_r )
      : _etcDir( std::move(etcDir_r ) )
      , _usrDir( std::move(usrDir_r ) )
      {}

      const std::optional<Pathname> & etcDir() const { return _etcDir; }
      const std::optional<Pathname> & usrDir() const { return _usrDir; }

      void getConfigFiles( Pathname config_r, const Pathname & root_r = Pathname("/") ) const
      {
        // config_r must denote a config file below some root dir:
        // [PROJECT/]EXAMPLE[.SUFFIX]
        if ( config_r.relative() ) {
          if ( config_r.relativeDotDot() )
            ZYPP_THROW( Exception(str::sprint("Config stem must not refer to '../':", config_r)) );
          config_r = config_r.absolutename();
        }
        if ( config_r.emptyOrRoot() )
          ZYPP_THROW( Exception(str::sprint("Config stem must not be empty:", config_r)) );

        pMIL( "getConfigFiles for stem", config_r, "below", root_r, "in", _etcDir, _usrDir );
        Pathname    configDir    { config_r.extend( ".d" ) }; // [PROJECT/]EXAMPLE[.SUFFIX].d
        std::string configSuffix { config_r.extension() };    // [.SUFFIX]

        // relevant lookup dirs in order of preference:
        std::vector<Pathname> lookupDirs;
        if ( _etcDir ) {  // system configuration
          PathInfo ldir { root_r / *_etcDir };
          if ( ldir.isDir() )
            lookupDirs.push_back( ldir.path() );
        }
        {                 // /run for ephemeral overrides
          PathInfo ldir { root_r / "/run" };
          if ( ldir.isDir() )
            lookupDirs.push_back( ldir.path() );
        }
        if ( _usrDir ) {  // vendor configuration
          PathInfo ldir { root_r / *_usrDir };
          if ( ldir.isDir() )
            lookupDirs.push_back( ldir.path() );
        }

        // now collect base config and drop-ins to parse:
        PathInfo                       basecfgToParse; // the full (base) config to parse (if any)
        std::map<std::string,PathInfo> dropinsToParse; // drop-ins to parse (1st. wins; lex sorted)

        for ( const Pathname & ldir : lookupDirs ) {
          // base config
          PathInfo basecfg { ldir / config_r };
          takeOrMask( basecfg, basecfgToParse );

          // dropins
          PathInfo dropinDir { ldir / configDir };
          if ( dropinDir.isDir() ) {
            filesystem::dirForEachExt( dropinDir.path(), [&]( const zypp::Pathname &p, const zypp::filesystem::DirEntry &entry ) {
              if ( entry.name[0] == '.' )
                return true;
              if ( not ( configSuffix.empty() || str::hasSuffix( entry.name, configSuffix ) ) )
                return true;
              PathInfo dropin { p / entry.name };
              takeOrMask( dropin, dropinsToParse[entry.name] );
              return true;
            } );
          }
        }
        pMIL( basecfgToParse );
        pMIL( dropinsToParse );
      }

    private:
      /** The 1st viable \a file_r is stored in \a toParse_r; later candidates are masked. */
      static void takeOrMask( const PathInfo & file_r, PathInfo & toParse_r )
      {
        if ( file_r.isExist() && not file_r.isDir() ) {
          if ( toParse_r.path().empty() ) {
            //pDBG( "take", file_r );
            toParse_r = std::move(file_r); //file_r.path();
          } else {
            pDBG( "masked", file_r, "by", toParse_r );
          }
        }
      }

    private:
      std::optional<Pathname> _etcDir;  ///< system configuration: /etc
      std::optional<Pathname> _usrDir;  ///< vendor configuration: distconfdir (/usr/etc)
    };

  } // namespace econf

  namespace parser {

    EconfDict::EconfDict( Pathname stem_r, const Pathname & root_r )
    : IniDict()
    { ; }

  } // namespace parser
} // namespace zypp

