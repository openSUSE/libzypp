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

#include <tuple>  // std::ignore
#include <optional>

#include <zypp-core/APIConfig.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/base/LogTools.h>

namespace zypp::parser {

  namespace econf {

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
      static Pathname _defaultDistconfDir; ///< APIConfig(LIBZYPP_ZYPPCONFDIR) but amendable for Testing

      ConfigurationContext()
      : ConfigurationContext( "/etc", _defaultDistconfDir )
      {}

      ConfigurationContext( std::optional<Pathname> etcDir_r, std::optional<Pathname> usrDir_r )
      : _etcDir( std::move(etcDir_r) )
      , _usrDir( std::move(usrDir_r) )
      {}

      const std::optional<Pathname> & etcDir() const { return _etcDir; }
      const std::optional<Pathname> & usrDir() const { return _usrDir; }

      std::vector<Pathname> getConfigFiles( const std::string & stem_r, const Pathname & root_r = Pathname("/") ) const
      {
        // configStem must denote a config file below some root dir:
        // [PROJECT/]EXAMPLE[.SUFFIX]
        Pathname    configStem { stem_r };
        if ( configStem.relative() ) {
          if ( configStem.relativeDotDot() )
            ZYPP_THROW( Exception(str::sprint("Config stem must not refer to '../':", configStem)) );
          configStem = configStem.absolutename();
        }
        if ( configStem.emptyOrRoot() )
          ZYPP_THROW( Exception(str::sprint("Config stem must not be empty:", configStem)) );
        pMIL( "getConfigFiles for stem", configStem, "below", root_r, "in", _etcDir, _usrDir );
        Pathname    configDir    { configStem.extend( ".d" ) }; // [PROJECT/]EXAMPLE[.SUFFIX].d
        std::string configSuffix { configStem.extension() };    // [.SUFFIX]

        // relevant lookup dirs in order of preference:
        std::vector<Pathname> lookupDirs;
        if ( _etcDir )
          takeLookupDirIf( lookupDirs, root_r / *_etcDir );   // system configuration
        takeLookupDirIf( lookupDirs, root_r / "/run" );       // /run for ephemeral overrides
        if ( _usrDir )
          takeLookupDirIf( lookupDirs, root_r / *_usrDir );   // vendor configuration

        // now collect base config and drop-ins to parse:
        PathInfo                       basecfgToParse; // the full (base) config to parse (if any)
        std::map<std::string,PathInfo> dropinsToParse; // drop-ins to parse (1st. wins; lex sorted)

        for ( const Pathname & ldir : lookupDirs ) {
          // base config
          PathInfo basecfg { ldir / configStem };
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

        // prepare the list to parse
        std::vector<Pathname> ret;
        handOutIf( ret, basecfgToParse );
        for ( const auto & [_,p] : dropinsToParse ) {
          std::ignore = _;  // silence warning: unused variable
          handOutIf( ret, p );
        }
        if ( ret.empty() )
          pMIL( "No config files found for stem", configStem, "below", root_r );
        return ret;
      }

    private:
      /** Remember valid lookup dirs. */
      static void takeLookupDirIf( std::vector<Pathname> & lookupDirs_r, Pathname dir_r )
      {
        if ( PathInfo(dir_r).isDir() )
          lookupDirs_r.push_back( dir_r );
      }

      /** An empty path denotes the unset value. */
      static bool isSet( const PathInfo & file_r )
      { return not file_r.path().empty(); }

      /** The 1st viable \a file_r is stored in \a toParse_r; candidates found later are masked. */
      static void takeOrMask( const PathInfo & file_r, PathInfo & toParse_r )
      {
        if ( file_r.isExist() && not file_r.isDir() ) {
          if ( isSet( toParse_r ) ) {
            pDBG( "masked", file_r, "by", toParse_r );
          } else {
            toParse_r = std::move(file_r); //file_r.path();
          }
        }
      }

      /** Store set values in \a ret_r. */
      static void handOutIf( std::vector<Pathname> & ret_r, const PathInfo & file_r )
      {
        if ( isSet( file_r ) ) {
          pMIL( "take", file_r );
          ret_r.push_back( file_r.path() );
        }
      }


    private:
      std::optional<Pathname> _etcDir;  ///< system configuration: /etc
      std::optional<Pathname> _usrDir;  ///< vendor configuration: distconfdir (APIConfig(LIBZYPP_ZYPPCONFDIR))
    };

    Pathname ConfigurationContext::_defaultDistconfDir { APIConfig(LIBZYPP_ZYPPCONFDIR) };

  } // namespace econf

  EconfDict::EconfDict()
  {}

  EconfDict::EconfDict( const std::string & stem_r, const Pathname & root_r )
  : IniDict()
  {
    for ( const Pathname & file : econf::ConfigurationContext().getConfigFiles( stem_r, root_r ) ) {
      read( file );
    }
    pDBG( stem_r, "below", root_r, ":", *this );
  }

  Pathname EconfDict::defaultDistconfDir()
  { return econf::ConfigurationContext::_defaultDistconfDir; }

  void EconfDict::defaultDistconfDir( Pathname path_r )
  { econf::ConfigurationContext::_defaultDistconfDir = std::move(path_r); }
} // namespace zypp::parser

