/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/parser/RepoFileReader.cc
 *
*/

#include "servicefilereader.h"
#include <iostream>
#include <zypp/base/Logger.h>
#include <zypp/base/String.h>
#include <zypp/base/Regex.h>
#include <zypp-core/base/InputStream>
#include <zypp-core/base/UserRequestException>

#include <zypp-core/parser/IniDict>
#include <zypp/ng/serviceinfo.h>

using std::endl;
using zypp::parser::IniDict;

///////////////////////////////////////////////////////////////////
namespace zyppng
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace parser
  { /////////////////////////////////////////////////////////////////

    class ServiceFileReader::Impl
    {
    public:
      static void parseServices( zyppng::ContextBaseRef ctx, const zypp::Pathname & file,
          const ServiceFileReader::ProcessService & callback );
    };

    void ServiceFileReader::Impl::parseServices( zyppng::ContextBaseRef ctx, const zypp::Pathname & file,
                                  const ServiceFileReader::ProcessService & callback/*,
                                  const ProgressData::ReceiverFnc &progress*/ )
    try {
      zypp::InputStream is(file);
      if( is.stream().fail() )
      {
        ZYPP_THROW(zypp::Exception("Failed to open service file"));
      }

      zypp::parser::IniDict dict(is);
      for ( zypp::parser::IniDict::section_const_iterator its = dict.sectionsBegin();
            its != dict.sectionsEnd();
            ++its )
      {
        MIL << (*its) << endl;

        zyppng::ServiceInfo service(ctx, *its);
        std::map<std::string,std::pair<std::string,ServiceInfo::RepoState>> repoStates;	// <repo_NUM,< alias,RepoState >>

        for ( IniDict::entry_const_iterator it = dict.entriesBegin(*its);
              it != dict.entriesEnd(*its);
              ++it )
        {
          // MIL << (*it).first << endl;
          if ( it->first == "name" )
            service.setName( it->second );
          else if ( it->first == "url" && ! it->second.empty() )
            service.setUrl( zypp::Url (it->second) );
          else if ( it->first == "enabled" )
            service.setEnabled( zypp::str::strToTrue( it->second ) );
          else if ( it->first == "autorefresh" )
            service.setAutorefresh( zypp::str::strToTrue( it->second ) );
          else if ( it->first == "type" )
            service.setType( zypp::repo::ServiceType(it->second) );
          else if ( it->first == "ttl_sec" )
            service.setTtl( zypp::str::strtonum<zypp::Date::Duration>(it->second) );
          else if ( it->first == "lrf_dat" )
            service.setLrf( zypp::Date( it->second ) );
          else if ( it->first == "repostoenable" )
          {
            std::vector<std::string> aliases;
            zypp::str::splitEscaped( it->second, std::back_inserter(aliases) );
            for_( ait, aliases.begin(), aliases.end() )
            {
              service.addRepoToEnable( *ait );
            }
          }
          else if ( it->first == "repostodisable" )
          {
            std::vector<std::string> aliases;
            zypp::str::splitEscaped( it->second, std::back_inserter(aliases) );
            for_( ait, aliases.begin(), aliases.end() )
            {
              service.addRepoToDisable( *ait );
            }
          }
          else if ( zypp::str::startsWith( it->first, "repo_" ) )
          {
            static zypp::str::regex rxexpr( "([0-9]+)(_(.*))?" );
            zypp::str::smatch what;
            if ( zypp::str::regex_match( it->first.c_str()+5/*repo_*/, what, rxexpr ) )
            {
              std::string tag( what[1] );
              if (  what.size() > 3 )
              {
                // attribute
                if ( what[3] == "enabled" )
                  repoStates[tag].second.enabled = zypp::str::strToBool( it->second, repoStates[tag].second.enabled );
                else if ( what[3] == "autorefresh" )
                  repoStates[tag].second.autorefresh = zypp::str::strToBool( it->second, repoStates[tag].second.autorefresh );
                else if ( what[3] == "priority" )
                  zypp::str::strtonum( it->second, repoStates[tag].second.priority );
                else
                  ERR << "Unknown attribute " << it->first << " ignored" << endl;
              }
              else
              {
                // alias
                repoStates[tag].first = it->second;
              }
            }
            else
              ERR << "Unknown attribute " << it->first << " ignored" << endl;
          }
          else
            ERR << "Unknown attribute " << it->first << " ignored" << endl;
        }

        if ( ! repoStates.empty() )
        {
          ServiceInfo::RepoStates data;
          for ( const auto & el : repoStates )
          {
            if ( el.second.first.empty() )
              ERR << "Missing alias for repo_" << el.first << "; ignore entry" << endl;
            else
              data[el.second.first] = el.second.second;
          }
          if ( ! data.empty() )
            service.setRepoStates( std::move(data) );
        }

        MIL << "Linking ServiceInfo with file " << file << endl;
        service.setFilepath(file);

        // add it to the list.
        if ( !callback(service) )
          ZYPP_THROW(zypp::AbortRequestException());
      }
    }
    catch ( zypp::Exception & ex ) {
      ex.addHistory( "Parsing .service file "+file.asString() );
      ZYPP_RETHROW( ex );
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : RepoFileReader
    //
    ///////////////////////////////////////////////////////////////////

    ServiceFileReader::ServiceFileReader(
                                    zyppng::ContextBaseRef ctx,
                                    const zypp::Pathname & repo_file,
                                    const ProcessService & callback/*,
                                    const ProgressData::ReceiverFnc &progress */)
    {
      Impl::parseServices(ctx, repo_file, callback/*, progress*/);
      //MIL << "Done" << endl;
    }

    ServiceFileReader::~ServiceFileReader()
    {}

    std::ostream & operator<<( std::ostream & str, const ServiceFileReader & obj )
    {
      return str;
    }

    /////////////////////////////////////////////////////////////////
  } // namespace parser
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
