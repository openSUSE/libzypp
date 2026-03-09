/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "solvableident.h"
#include <zypp-core/base/String.h>
#include <cstring>

namespace zyppng::sat
{
    namespace
    {
      void _doSplit( IdString & _ident, ResKind & _kind, IdString & _name )
      {
        if ( ! _ident )
          return;

        ResKind explicitKind = ResKind::explicitBuiltin( _ident.c_str() );
        // NOTE: kind package and srcpackage do not have namespaced ident!
        if ( ! explicitKind  )
        {
          _name = _ident;
          // No kind defaults to package
          if ( !_kind )
            _kind = ResKind::package;
          else if ( ! ( _kind == ResKind::package || _kind == ResKind::srcpackage ) )
            _ident = IdString( zypp::str::form( "%s:%s", _kind.c_str(), _ident.c_str() ) );
        }
        else
        {
          // strip kind spec from name
          _name = IdString( ::strchr( _ident.c_str(), ':' )+1 );
          _kind = explicitKind;
          if ( _kind == ResKind::package || _kind == ResKind::srcpackage )
            _ident = _name;
        }
        return;
      }
    } // namespace

    SplitIdent::SplitIdent( IdString ident_r )
    : _ident( ident_r )
    { _doSplit( _ident, _kind, _name ); }

    SplitIdent::SplitIdent( const char * ident_r )
    : _ident( ident_r )
    { _doSplit( _ident, _kind, _name ); }

    SplitIdent::SplitIdent( const std::string & ident_r )
    : _ident( ident_r )
    { _doSplit( _ident, _kind, _name ); }

    SplitIdent::SplitIdent( ResKind kind_r, IdString name_r )
    : _ident( name_r )
    , _kind(std::move( kind_r ))
    { _doSplit( _ident, _kind, _name ); }

    SplitIdent::SplitIdent( ResKind kind_r, const zypp::C_Str & name_r )
    : _ident( name_r )
    , _kind(std::move( kind_r ))
    { _doSplit( _ident, _kind, _name ); }

} // namespace zyppng::sat
