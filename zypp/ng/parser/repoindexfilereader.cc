/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/ng/parser/repoindexfilereader.cc
 * Implementation of repoindex.xml file reader.
 */

#include "repoindexfilereader.h"

#include <iostream>
#include <unordered_map>
#include <utility>

#include <zypp/base/String.h>
#include <zypp/base/Logger.h>
#include <zypp/base/Gettext.h>
#include <zypp-core/base/InputStream>
#include <zypp-core/base/DefaultIntegral>

#include <zypp/Pathname.h>

#include <zypp/parser/xml/Reader.h>
#include <zypp-core/parser/ParseException>

#include <zypp/ng/repoinfo.h>


#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "parser"

using std::endl;

namespace zyppng
{
  namespace parser
  {
    class RepoIndexVarReplacer
    {
    public:
      RepoIndexVarReplacer() = default;
      ~RepoIndexVarReplacer() = default;
      RepoIndexVarReplacer(const RepoIndexVarReplacer &) = delete;
      RepoIndexVarReplacer(RepoIndexVarReplacer &&) = delete;
      RepoIndexVarReplacer &operator=(const RepoIndexVarReplacer &) = delete;
      RepoIndexVarReplacer &operator=(RepoIndexVarReplacer &&) = delete;
      /** */
      void setVar( const std::string & key_r, const std::string & val_r )
      {
        //MIL << "*** Inject " << key_r << " = " << val_r;
        _vars[key_r] = replace( val_r );
        //MIL << " (" << _vars[key_r] << ")" << endl;
      }

      std::string replace( const std::string & val_r ) const
      {
        std::string::size_type vbeg = val_r.find( "%{", 0 );
        if ( vbeg == std::string::npos )
          return val_r;

        zypp::str::Str ret;
        std::string::size_type cbeg = 0;
        for( ; vbeg != std::string::npos; vbeg = val_r.find( "%{", vbeg ) )
        {
          std::string::size_type nbeg = vbeg+2;
          std::string::size_type nend = val_r.find( '}', nbeg );
          if ( nend == std::string::npos )
          {
            WAR << "Incomplete variable in '" << val_r << "'" << endl;
            break;
          }
          const auto & iter = _vars.find( val_r.substr( nbeg, nend-nbeg ) );
          if ( iter != _vars.end() )
          {
            if ( cbeg < vbeg )
              ret << val_r.substr( cbeg, vbeg-cbeg );
            ret << iter->second;
            cbeg = nend+1;
          }
          else
            WAR << "Undefined variable %{" << val_r.substr( nbeg, nend-nbeg ) << "} in '" << val_r << "'" << endl;
          vbeg = nend+1;
        }
        if ( cbeg < val_r.size() )
          ret << val_r.substr( cbeg );

        return ret;
      }
    private:
      std::unordered_map<std::string,std::string> _vars;
    };

  bool RepoIndexFileReader::getAttrValue( const std::string & key_r, zypp::xml::Reader & reader_r, std::string & value_r )
  {
    const zypp::xml::XmlString & s( reader_r->getAttribute( key_r ) );
    if ( s.get() )
    {
      value_r = _replacer->replace( s.asString() );
      return !value_r.empty();
    }
    value_r.clear();
    return false;
  }


  // --------------------------------------------------------------------------

  /*
   * xpath and multiplicity of processed nodes are included in the code
   * for convenience:
   *
   * // xpath: <xpath> (?|*|+)
   *
   * if multiplicity is ommited, then the node has multiplicity 'one'.
   */

  // --------------------------------------------------------------------------

  bool RepoIndexFileReader::consumeNode( zypp::xml::Reader & reader_r )
  {
    if ( reader_r->nodeType() == XML_READER_TYPE_ELEMENT )
    {
      // xpath: /repoindex
      if ( reader_r->name() == "repoindex" )
      {
        while ( reader_r.nextNodeAttribute() )
        {
          const std::string & name( reader_r->localName().asString() );
          const std::string & value( reader_r->value().asString() );
          _replacer->setVar( name, value );
          // xpath: /repoindex@ttl
          if ( name == "ttl" )
            _ttl = zypp::str::strtonum<zypp::Date::Duration>(value);
        }
        return true;
      }

      // xpath: /repoindex/data (+)
      if ( reader_r->name() == "repo" )
      {
        RepoInfo info(_zyppCtx);
        // Set some defaults that are not contained in the repo information
        info.setAutorefresh( true );
        info.setEnabled(false);

        std::string attrValue;

        // required alias
        // mandatory, so we can allow it in var replacement without reset
        if ( getAttrValue( "alias", reader_r, attrValue ) )
        {
          info.setAlias( attrValue );
          _replacer->setVar( "alias", attrValue );
        }
        else
          throw zypp::parser::ParseException(zypp::str::form(_("Required attribute '%s' is missing."), "alias"));

        // required url
        // SLES HACK: or path, but beware of the hardcoded '/repo' prefix!
        {
          std::string urlstr;
          std::string pathstr;
          getAttrValue( "url", reader_r, urlstr );
          getAttrValue( "path", reader_r, pathstr );
          if ( urlstr.empty() )
          {
            if ( pathstr.empty() )
              throw zypp::parser::ParseException(zypp::str::form(_("One or both of '%s' or '%s' attributes is required."), "url", "path"));
            else
              info.setPath( zypp::Pathname("/repo") / pathstr );
          }
          else
          {
            if ( pathstr.empty() )
              info.setBaseUrl( zypp::Url(urlstr) );
            else
            {
              zypp::Url url( urlstr );
              url.setPathName( zypp::Pathname(url.getPathName()) / "repo" / pathstr );
              info.setBaseUrl( url );
            }
          }
        }

        // optional name
        if ( getAttrValue( "name", reader_r, attrValue ) )
          info.setName( attrValue );

        // optional targetDistro
        if ( getAttrValue( "distro_target", reader_r, attrValue ) )
          info.setTargetDistribution( attrValue );

        // optional priority
        if ( getAttrValue( "priority", reader_r, attrValue ) )
          info.setPriority( zypp::str::strtonum<unsigned>( attrValue ) );


        // optional enabled
        if ( getAttrValue( "enabled", reader_r, attrValue ) )
          info.setEnabled( zypp::str::strToBool( attrValue, info.enabled() ) );

        // optional autorefresh
        if ( getAttrValue( "autorefresh", reader_r, attrValue ) )
          info.setAutorefresh( zypp::str::strToBool( attrValue, info.autorefresh() ) );

        DBG << info << endl;

        // ignore the rest
        _callback(info);
        return true;
      }
    }

    return true;
  }


  ///////////////////////////////////////////////////////////////////
  //
  //  CLASS NAME : RepoindexFileReader
  //
  ///////////////////////////////////////////////////////////////////

  RepoIndexFileReader::RepoIndexFileReader( ContextBaseRef ctx, zypp::Pathname repoindex_file, ProcessResource callback )
    : _zyppCtx( std::move(ctx) )
    , _callback( std::move(callback) )
    , _replacer( std::make_unique<RepoIndexVarReplacer>() )
  { run( zypp::InputStream(std::move(repoindex_file)) ); }

  RepoIndexFileReader::RepoIndexFileReader( ContextBaseRef ctx, const zypp::InputStream &is, ProcessResource callback )
    : _zyppCtx( std::move(ctx) )
    , _callback( std::move(callback) )
    , _replacer( std::make_unique<RepoIndexVarReplacer>() )
  { run( is ); }

  RepoIndexFileReader::~RepoIndexFileReader()
  {}

  zypp::Date::Duration RepoIndexFileReader::ttl() const	{ return _ttl; }

  void RepoIndexFileReader::run( const zypp::InputStream &is )
  {
    zypp::xml::Reader reader( is );
    MIL << "Reading " << is.path() << endl;
    reader.foreachNode( [this]( auto &reader ) { return consumeNode (reader); } );
  }

  } // ns parser
} // ns zypp

// vim: set ts=2 sts=2 sw=2 et ai:
