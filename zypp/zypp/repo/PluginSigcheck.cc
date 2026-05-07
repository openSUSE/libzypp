/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/PluginSigcheck.cc
 */
#include <iostream>
#include <vector>

#include "PluginSigcheck.h"
#include <zypp-core/base/StringV.h>
#include <zypp-core/base/LogTools.h>
#include <zypp/PluginScript.h>

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::plugin"

namespace zypp
{
  namespace {
    struct ProtocolError : SigcheckPluginException {
      ProtocolError( const PluginScript & script_r, const PluginFrame & got_r, std::string_view expected_r )
      : SigcheckPluginException(str::sprint( script_r.script(),"protocol error: got",got_r.command(),"expected",expected_r ))
      {
        if ( got_r.isErrorCommand() && not got_r.body().empty() ) {
          addHistory( got_r.body().asString( 2048 ) );
        }
      }
    };
  } // namespace

  /**
   * \ingroup SIGCHECKPLUGIN
   */
  struct SigcheckPlugin::Impl
  {
    Impl( Pathname script_r )
    : _script( std::move(script_r) )
    {}
    Impl( Pathname script_r, Args args_r )
    : _script( std::move(script_r), std::move(args_r) )
    {}

    void launch( const Pathname & chroot_r )
    {
      if ( _script.isOpen() )
        ZYPP_THROW( SigcheckPluginException(str::sprint( _script.script(),"already launched" )) );

      try {
        _script.open();
        {
          // -> PLUGINBEGIN
          PluginFrame frame( "PLUGINBEGIN", { {"version","0"} } );
          _script.send( frame );
          PluginFrame ret = _script.receive();
          pDBG( dump(ret) );

          // <- PLUGINSETUP (optional)
          if ( not ret.isAckCommand() ) {
            // Optional PLUGINSETUP
            if ( ret.command() != "PLUGINSETUP" ) {
              ZYPP_THROW( ProtocolError( _script, ret, "PLUGINSETUP" ) );
            }
            if ( ret.hasKey( "sig_extension" ) )
              _sigExtension = ret.getHeader( "sig_extension" );
            if ( ret.hasKey( "key_extension" ) )
              _keyExtension = ret.getHeader( "key_extension" );
          }
        }
      }
      catch( const zypp::Exception & e ) {
        _script.close();
        ZYPP_THROW( SigcheckPluginException(str::sprint( "failed to launch",_script.script() ), e ) );
      }

      pMIL( "launch", chroot_r, *this );
    }

    void sigcheck( const Pathname & data_r, const Pathname & sig_r = Pathname(), const Pathname & key_r = Pathname() ) const
    {
      try {
        // -> PLUGINBEGIN
        PluginFrame frame( "SIGCHECK" );
        frame.addHeader( "data", data_r.asString() );
        if ( not sig_r.empty() )
          frame.addHeader( "sig", sig_r.asString() );
        if ( not key_r.empty() )
          frame.addHeader( "key", key_r.asString() );

        _script.send( frame );
        PluginFrame ret = _script.receive();
        pDBG( dump(ret) );

        if ( not ret.isAckCommand() ) {
          ZYPP_THROW( ProtocolError( _script, ret, "ACK" ) );
        }
      }
      catch( const zypp::Exception & e ) {
        ZYPP_THROW( SigcheckPluginException(str::sprint( _script.script(),"FAILED" ), e ) );
      }
    }

    PluginScript _script;
    std::string _sigExtension;
    std::string _keyExtension;
  };

  std::ostream & operator<<( std::ostream & str, const SigcheckPlugin::Impl & obj )
  { return str << obj._script; }

  ///////////////////////////////////////////////////////////////////

  SigcheckPlugin::SigcheckPlugin( Pathname script_r )
  : _pimpl { new Impl( std::move(script_r) ) }
  {}

  SigcheckPlugin::SigcheckPlugin( Pathname script_r, Args args_r )
  : _pimpl { new Impl( std::move(script_r), std::move(args_r) ) }
  {}

  void SigcheckPlugin::launch( const Pathname & chroot_r )
  { return _pimpl->launch( chroot_r ); }

  std::string SigcheckPlugin::sigExtension() const
  { return _pimpl->_sigExtension; }

  std::string SigcheckPlugin::keyExtension() const
  { return _pimpl->_keyExtension; }

  void SigcheckPlugin::sigcheck( const Pathname & data_r, const Pathname & sig_r, const Pathname & key_r ) const
  { return _pimpl->sigcheck( data_r, sig_r, key_r ); }

  std::ostream & operator<<( std::ostream & str, const SigcheckPlugin & obj )
  { return str << *obj._pimpl; }

  ///////////////////////////////////////////////////////////////////

  struct SigcheckPlugins::Impl
  {
    Impl()
    {}

    Impl( const std::string & cmdline_r, const Pathname & plugindir_r )
    {
      for ( std::string_view cmd : strv::splitBSEscaped( cmdline_r, ";" ) ) {
        Pathname             plugpath;
        SigcheckPlugin::Args plugargs;
        for ( std::string_view arg : strv::splitBSEscaped( cmd /*blank*/ ) ) {
          if ( arg.empty() )
            continue;
          if ( plugpath.empty() ) {
            plugpath = plugindir_r / "sigcheck" / std::string(arg);
          } else {
            plugargs.emplace_back( strv::unBSEscape( arg ) );
          }
        }
        if ( not plugpath.empty() ) {
          if ( plugargs.empty() )
            _plugins.emplace_back( std::move(plugpath) );
          else
            _plugins.emplace_back( std::move(plugpath), std::move(plugargs) );
        }
      }

      Plugins _plugins; ///< NOTE: assert it's not reallocated after construction
    }

    explicit operator bool() const
    { return not _plugins.empty();   }

    void launch( const Pathname & chroot_r = Pathname() )
    { for ( auto & plugin : _plugins ) plugin.launch( chroot_r ); }

    Plugins _plugins; ///< \note Assert the vector is never reallocated!
  };

  std::ostream & operator<<( std::ostream & str, const SigcheckPlugins::Impl & obj )
  { return str << obj._plugins; }

  ///////////////////////////////////////////////////////////////////

  SigcheckPlugins::SigcheckPlugins()
  : _pimpl { new Impl() }
  {}

  SigcheckPlugins::SigcheckPlugins( const std::string & cmdline_r, const Pathname & plugindir_r )
  : _pimpl { new Impl( cmdline_r, plugindir_r ) }
  {}

  SigcheckPlugins::operator bool() const
  { return bool(*_pimpl); }

  const SigcheckPlugins::Plugins & SigcheckPlugins::plugins()
  { return _pimpl->_plugins; }

  void SigcheckPlugins::launch( const Pathname & chroot_r )
  { return _pimpl->launch( chroot_r ); }

  std::ostream & operator<<( std::ostream & str, const SigcheckPlugins & obj )
  { return str << *obj._pimpl; }

} // namespace zypp
