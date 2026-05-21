/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/parser/susetags/ContentFileReader.cc
 *
*/
#include <iostream>
#include <sstream>

#include <zypp/ZYppCallbacks.h>
#include <zypp-core/base/LogTools.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/IOStream.h>
#include <zypp-core/base/UserRequestException>
#include <zypp-core/parser/ParseException>

#include <zypp/parser/susetags/ContentFileReader.h>
#include <zypp/parser/susetags/RepoIndex.h>

using std::endl;
#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "parser::susetags"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace parser
  { /////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////
    namespace susetags
    { /////////////////////////////////////////////////////////////////

      namespace {
        // Take care the parsed pathnames do not
        // refer to locations outside the repo!
        Pathname sanitize( Pathname path_r )
        {
          Pathname ret = path_r.absolutename();   // strips leading ../s.
          if ( path_r.relativeDotDot() ) {
            // Don't accept downloadable data outside repo root
            JobReport::warning( str::sconcat( "Content file: hostile location ",path_r," => ", ret ) );
            pWAR( "Hostile location:", path_r, "=>", ret );
          }
          return ret;
        }

        std::string sanitizeEntry( Pathname path_r )
        {
          if ( path_r.empty() )
            return {};
          // HASH SHA1 d423ad41e93a51195a6264961e4a074c6d89359d  boot/../x86_64/bind    => x86_64/bind
          // HASH SHA1 d423ad41e93a51195a6264961e4a074c6d89359d  boot/../../x86_64/bind => ../* discarded
          // Turning it into a Pathname normalizes the representation.
          if ( path_r.relativeDotDot() ) {
            // Don't accept downloadable data outside repo root
            JobReport::warning( str::sconcat(  "Content file: hostile location ",path_r," => discard data entry" ) );
            pWAR( "Hostile location:", path_r, "=>", "discard data entry" );
            return {};
          }
          return path_r.asString().substr( path_r.absolute() ? 1 : 2 ); // skip leading "/" or  "./"
        }
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	CLASS NAME : ContentFileReader::Impl
      //
      /** ContentFileReader implementation. */
      struct ContentFileReader::Impl
      {
        public:
          Impl()
          {}

          RepoIndex & repoindex()
          {
            if ( !_repoindex )
              _repoindex = new RepoIndex;
            return *_repoindex;
          }

          bool hasRepoIndex() const
          { return _repoindex != nullptr; }

          RepoIndex_Ptr handoutRepoIndex()
          {
            RepoIndex_Ptr ret;
            ret.swap( _repoindex );
            _repoindex = nullptr;
            return ret;
          }

        public:
          bool setFileCheckSum( std::map<std::string, CheckSum> & map_r, const std::string & value ) const
          {
            bool error = false;
            std::vector<std::string> words;
            if ( str::split( value, std::back_inserter( words ) ) == 3 )
            {
              std::string pathstr = sanitizeEntry( words[2] );
              if ( not pathstr.empty() )
                map_r[std::move(pathstr)] = CheckSum( words[0], words[1] );
            }
            else
            {
              error = true;
            }
            return error;
          }

        public:
          std::string _inputname;

        private:
          RepoIndex_Ptr      _repoindex;
      };
      ///////////////////////////////////////////////////////////////////

      ///////////////////////////////////////////////////////////////////
      //
      //	CLASS NAME : ContentFileReader
      //
      ///////////////////////////////////////////////////////////////////

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : ContentFileReader::ContentFileReader
      //	METHOD TYPE : Ctor
      //
      ContentFileReader::ContentFileReader()
      {}

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : ContentFileReader::~ContentFileReader
      //	METHOD TYPE : Dtor
      //
      ContentFileReader::~ContentFileReader()
      {}

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : ContentFileReader::beginParse
      //	METHOD TYPE : void
      //
      void ContentFileReader::beginParse()
      {
        _pimpl.reset( new Impl() );
        // actually mandatory, but in case they were forgotten...
        _pimpl->repoindex().descrdir = "suse/setup/descr";
        _pimpl->repoindex().datadir = "suse";
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : ContentFileReader::endParse
      //	METHOD TYPE : void
      //
      void ContentFileReader::endParse()
      {
        // consume oldData
        if ( _pimpl->hasRepoIndex() )
        {
          if ( _repoIndexConsumer )
            _repoIndexConsumer( _pimpl->handoutRepoIndex() );
        }

        MIL << "[Content]" << endl;
        _pimpl.reset();
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : ContentFileReader::userRequestedAbort
      //	METHOD TYPE : void
      //
      void ContentFileReader::userRequestedAbort( unsigned lineNo_r )
      {
        ZYPP_THROW( AbortRequestException( errPrefix( lineNo_r ) ) );
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : ContentFileReader::errPrefix
      //	METHOD TYPE : std::string
      //
      std::string ContentFileReader::errPrefix( unsigned lineNo_r,
                                                const std::string & msg_r,
                                                const std::string & line_r ) const
      {
        return str::form( "%s:%u:%s | %s",
                          _pimpl->_inputname.c_str(),
                          lineNo_r,
                          line_r.c_str(),
                          msg_r.c_str() );
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : ContentFileReader::parse
      //	METHOD TYPE : void
      //
      void ContentFileReader::parse( const InputStream & input_r,
                                     const ProgressData::ReceiverFnc & fnc_r )
      {
        MIL << "Start parsing content repoindex" << input_r << endl;
        if ( ! input_r.stream() )
        {
          std::ostringstream s;
          s << "Can't read bad stream: " << input_r;
          ZYPP_THROW( ParseException( s.str() ) );
        }
        beginParse();
        _pimpl->_inputname = input_r.name();

        ProgressData ticks( makeProgressData( input_r ) );
        ticks.sendTo( fnc_r );
        if ( ! ticks.toMin() )
          userRequestedAbort( 0 );

        iostr::EachLine line( input_r );
        for( ; line; line.next() )
        {
          // strip 1st word from line to separate tag and value.
          std::string value( *line );
          std::string key( str::stripFirstWord( value, /*ltrim_first*/true ) );

          if ( key.empty() || *key.c_str() == '#' ) // empty or comment line
          {
            continue;
          }

          // strip modifier if exists
          std::string modifier;
          std::string::size_type pos = key.rfind( '.' );
          if ( pos != std::string::npos )
          {
            modifier = key.substr( pos+1 );
            key.erase( pos );
          }

          //
          // ReppoIndex related data:
          //
          else if ( key == "DESCRDIR" )
          {
            _pimpl->repoindex().descrdir = sanitize( value );
          }
          else if ( key == "DATADIR" )
          {
            _pimpl->repoindex().datadir = sanitize( value );
          }
          else if ( key == "KEY" )
          {
            if ( _pimpl->setFileCheckSum( _pimpl->repoindex().signingKeys, value ) )
            {
              ZYPP_THROW( ParseException( errPrefix( line.lineNo(), "Expected [KEY algorithm checksum filename]", *line ) ) );
            }
          }
          else if ( key == "META" )
          {
            if ( _pimpl->setFileCheckSum( _pimpl->repoindex().metaFileChecksums, value ) )
            {
              ZYPP_THROW( ParseException( errPrefix( line.lineNo(), "Expected [algorithm checksum filename]", *line ) ) );
            }
          }
          else if ( key == "HASH" )
          {
            if ( _pimpl->setFileCheckSum( _pimpl->repoindex().mediaFileChecksums, value ) )
            {
              ZYPP_THROW( ParseException( errPrefix( line.lineNo(), "Expected [algorithm checksum filename]", *line ) ) );
            }
          }
          else
          {
            DBG << errPrefix( line.lineNo(), "ignored", *line ) << endl;
          }


          if ( ! ticks.set( input_r.stream().tellg() ) )
            userRequestedAbort( line.lineNo() );
        }

        //
        // post processing
        //
        if ( ! ticks.toMax() )
          userRequestedAbort( line.lineNo() );

        endParse();
        MIL << "Done parsing " << input_r << endl;
      }

      /////////////////////////////////////////////////////////////////
    } // namespace susetags
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////
  } // namespace parser
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
