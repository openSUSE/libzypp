/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/parser/RepoindexFileReader.cc
 * Implementation of repoindex.xml file reader.
 */
#include <iostream>
#include <unordered_map>

#include <zypp-core/base/String.h>
#include <zypp-core/base/Logger.h>
#include <zypp-core/base/Gettext.h>
#include <utility>
#include <zypp-core/base/InputStream>
#include <zypp-core/base/DefaultIntegral>

#include <zypp-core/Pathname.h>

#include <zypp/parser/xml/Reader.h>
#include <zypp-core/parser/ParseException>

#include <zypp/RepoInfo.h>

#include <zypp/parser/RepoindexFileReader.h>


#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "parser"

using std::endl;

namespace zypp
{
  namespace parser
  {
    using xml::Reader;
    using xml::XmlString;

    ///////////////////////////////////////////////////////////////////
    namespace
    {
      class VarReplacer : private base::NonCopyable
      {
      public:
        /** Global variables set for all repos. */
        void setGlobalVar( const std::string & key_r, const std::string & val_r )
        { _global[key_r] = replace( val_r ); }

        /** Local variables set for one repo. */
        void setSectionVar( const std::string & key_r, const std::string & val_r )
        { _section[key_r] = replace( val_r ); }

        /** Start a new repo section. */
        void clearSectionVars()
        { _section.clear(); }

        std::string replace( const std::string & val_r ) const
        {
          std::string::size_type vbeg = val_r.find( "%{", 0 );
          if ( vbeg == std::string::npos )
            return val_r;

          str::Str ret;
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
            const std::string * varptr = getVar( val_r.substr( nbeg, nend-nbeg ) );
            if ( varptr )
            {
              if ( cbeg < vbeg )
                ret << val_r.substr( cbeg, vbeg-cbeg );
              ret << *varptr;
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
        const std::string * getVar( const std::string & key_r ) const
        {
          const std::string * ret = getVar( key_r, _section );
          if ( not ret )
            ret = getVar( key_r, _global );
          return ret;
        }

        const std::string * getVar( const std::string & key_r, const std::unordered_map<std::string,std::string> & map_r ) const
        {
          const auto & iter = map_r.find( key_r );
          if ( iter != map_r.end() )
            return &(iter->second);
          return nullptr;
        }

      private:
        std::unordered_map<std::string,std::string> _global;
        std::unordered_map<std::string,std::string> _section;
      };
    } // namespace
    ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////////
  //
  //  CLASS NAME : RepoindexFileReader::Impl
  //
  class RepoindexFileReader::Impl : private base::NonCopyable
  {
  public:
    /**
     * CTOR
     *
     * \see RepoindexFileReader::RepoindexFileReader(Pathname,ProcessResource)
     */
    Impl(const InputStream &is, ProcessResource &&callback);

    /**
     * Callback provided to the XML parser.
     */
    bool consumeNode( Reader & reader_r );

    DefaultIntegral<Date::Duration,0> _ttl;

  private:
    bool getAttrValue( const std::string & key_r, Reader & reader_r, std::string & value_r )
    {
      const XmlString & s( reader_r->getAttribute( key_r ) );
      if ( s.get() )
      {
        value_r = _replacer.replace( s.asString() );
        return !value_r.empty();
      }
      value_r.clear();
      return false;
    }

  private:
    /** Function for processing collected data. Passed-in through constructor. */
    ProcessResource _callback;
    VarReplacer _replacer;
  };
  ///////////////////////////////////////////////////////////////////////

  RepoindexFileReader::Impl::Impl(const InputStream &is,
                                  ProcessResource &&callback)
    : _callback(std::move(callback))
  {
    Reader reader( is );
    MIL << "Reading " << is.path() << endl;
    reader.foreachNode( bind( &RepoindexFileReader::Impl::consumeNode, this, _1 ) );
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

  bool RepoindexFileReader::Impl::consumeNode( Reader & reader_r )
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
          _replacer.setGlobalVar( name, value );
          // xpath: /repoindex@ttl
          if ( name == "ttl" )
            _ttl = str::strtonum<Date::Duration>(value);
        }
        return true;
      }

      // xpath: /repoindex/data (+)
      if ( reader_r->name() == "repo" )
      {
        // TODO: Ideally the repo values here are interpreted the same way as
        // corresponding ones in the RepoFileReader. Check whether we can introduce
        // a RepoInfo ctor from a string dict. Using it here and in RepoFileReader.
        // For now we try to parse the same way as in RepoFileReader.

        RepoInfo info;
        // Set some defaults that are not contained in the repo information
        info.setAutorefresh( true );
        info.setEnabled(false);

        std::string attrValue;
        _replacer.clearSectionVars();

        // required alias
        // mandatory, so we can allow it in var replacement without reset
        if ( getAttrValue( "alias", reader_r, attrValue ) )
        {
          info.setAlias( attrValue );
          _replacer.setSectionVar( "alias", attrValue );
        }
        else
          throw ParseException(str::form(_("Required attribute '%s' is missing."), "alias"));

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
              // TODO: In fact we'd like to pass and check for url or mirrorlist at the end (check below).
              // But there is legacy code in serviceswf.cc where an empty baseurl is replaced by
              // the service Url. We need to check what kind of feature hides behind this code.
              // So for now we keep requiring an url attribut.
              throw ParseException(str::form(_("One or both of '%s' or '%s' attributes is required."), "url", "path"));
            else
              info.setPath( Pathname("/repo") / pathstr );
          }
          else
          {
            if ( pathstr.empty() )
              info.setBaseUrl( Url(urlstr) );
            else
            {
              Url url( urlstr );
              url.setPathName( Pathname(url.getPathName()) / "repo" / pathstr );
              info.setBaseUrl( url );
            }
            _replacer.setSectionVar( "url", urlstr );
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
          info.setPriority( str::strtonum<unsigned>( attrValue ) );

        // optional enabled
        if ( getAttrValue( "enabled", reader_r, attrValue ) )
          info.setEnabled( str::strToBool( attrValue, info.enabled() ) );

        // optional autorefresh
        if ( getAttrValue( "autorefresh", reader_r, attrValue ) )
          info.setAutorefresh( str::strToBool( attrValue, info.autorefresh() ) );

        // optional *gpgcheck
        if ( getAttrValue( "gpgcheck", reader_r, attrValue ) )
          info.setGpgCheck( str::strToTriBool( attrValue ) );
        if ( getAttrValue( "repo_gpgcheck", reader_r, attrValue ) )
          info.setRepoGpgCheck( str::strToTrue( attrValue ) );
        if ( getAttrValue( "pkg_gpgcheck", reader_r, attrValue ) )
          info.setPkgGpgCheck( str::strToTrue( attrValue ) );

        // optional keeppackages
        if ( getAttrValue( "keeppackages", reader_r, attrValue ) )
          info.setKeepPackages( str::strToTrue( attrValue ) );

        // optional gpgkey
        if ( getAttrValue( "gpgkey", reader_r, attrValue ) )
          info.setGpgKeyUrl( Url(attrValue) );

        // optional mirrorlist
        if ( getAttrValue( "mirrorlist", reader_r, attrValue ) )
          info.setMirrorlistUrl( Url(attrValue) );

        // optional metalink
        if ( getAttrValue( "metalink", reader_r, attrValue ) )
          info.setMetalinkUrl( Url(attrValue) );

        DBG << info << endl;
        // require at least one Url
        // NOTE: Some legacy feature in serviceswf.cc allows a path without baseurl and
        // combines this with the service url. That's why we require empty url and path
        // in order to fail.
        if ( info.baseUrlsEmpty() && info.path().empty() && not info.mirrorListUrl().isValid() ) {
          throw ParseException(str::form(_("Neither '%s' nor '%s' nor '%s' attribute is defined for service repo '%s'."),
                                           "url", "mirrorlist", "metalink", info.alias().c_str()));
        }

        // ignore the rest
        // Specially: bsc#1177427 et.al.: type in a .repo file is legacy - ignore it
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

  RepoindexFileReader::RepoindexFileReader( Pathname repoindex_file, ProcessResource callback )
  : _pimpl(new Impl( InputStream(std::move(repoindex_file)), std::move(callback) ))
  {}

  RepoindexFileReader::RepoindexFileReader(const InputStream &is, ProcessResource callback )
  : _pimpl(new Impl( is, std::move(callback) ))
  {}

  RepoindexFileReader::~RepoindexFileReader()
  {}

  Date::Duration RepoindexFileReader::ttl() const	{ return _pimpl->_ttl; }

  } // ns parser
} // ns zypp

// vim: set ts=2 sts=2 sw=2 et ai:
