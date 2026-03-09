/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/Capability.cc
 *
*/

#include "capability.h"

#include <iostream>
#include <zypp-core/base/Logger.h>

#include <zypp-core/base/String.h>
#include <zypp-core/base/Regex.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/base/Exception.h>

#include <zypp/ng/arch.h>
#include <zypp/ng/rel.h>
#include <zypp/ng/edition.h>
#include <zypp/ng/sat/stringpool.h>
#include <zypp/ng/sat/solvable.h>
#include <zypp/ng/sat/solvableident.h>
#include <zypp/ng/sat/cap2str.h>

using std::endl;

namespace zyppng::sat
{

  using zyppng::sat::StringPool;

  namespace
  {
    /// Round Robin string buffer
    template <unsigned TLen = 5>
    struct TempStrings
    {
      /** Reference to the next (cleared) tempstring. */
      std::string & getNext()
      { unsigned c = _next; _next = (_next+1) % TLen; _buf[c].clear(); return _buf[c]; }

    private:
      unsigned _next = 0;
      std::string _buf[TLen];
    };

    /** backward skip whitespace starting at pos_r */
    inline std::string::size_type backskipWs( const std::string & str_r, std::string::size_type pos_r )
    {
      for ( ; pos_r != std::string::npos; --pos_r )
      {
        char ch = str_r[pos_r];
        if ( ch != ' ' && ch != '\t' )
          break;
      }
      return pos_r;
    }

    /** backward skip non-whitespace starting at pos_r */
    inline std::string::size_type backskipNWs( const std::string & str_r, std::string::size_type pos_r )
    {
      for ( ; pos_r != std::string::npos; --pos_r )
      {
        char ch = str_r[pos_r];
        if ( ch == ' ' || ch == '\t' )
          break;
      }
      return pos_r;
    }

    /** Split any 'op edition' from str_r */
    void splitOpEdition( std::string & str_r, Rel & op_r, Edition & ed_r )
    {
      if ( str_r.empty() )
        return;
      std::string::size_type ch( str_r.size()-1 );

      // check whether the one but last word is a valid Rel:
      if ( (ch = backskipWs( str_r, ch )) != std::string::npos )
      {
        std::string::size_type ee( ch );
        if ( (ch = backskipNWs( str_r, ch )) != std::string::npos )
        {
          std::string::size_type eb( ch );
          if ( (ch = backskipWs( str_r, ch )) != std::string::npos )
          {
            std::string::size_type oe( ch );
            ch = backskipNWs( str_r, ch ); // now before 'op'? begin
            if ( op_r.parseFrom( str_r.substr( ch+1, oe-ch ) ) )
            {
              // found a legal 'op'
              ed_r = Edition( str_r.substr( eb+1, ee-eb ) );
              if ( ch != std::string::npos ) // 'op' is not at str_r begin, so skip WS
                ch = backskipWs( str_r, ch );
              str_r.erase( ch+1 );
              return;
            }
          }
        }
      }
      // HERE: Didn't find 'name op edition'
      // As a convenience we check for an embeded 'op' (not surounded by WS).
      // But just '[<=>]=?|!=' and not inside '()'.
      ch = str_r.find_last_of( "<=>)" );
      if ( ch != std::string::npos && str_r[ch] != ')' )
      {
        std::string::size_type oe( ch );

        // do edition first:
        ch = str_r.find_first_not_of( " \t", oe+1 );
        if ( ch != std::string::npos )
          ed_r = Edition( str_r.substr( ch ) );

        // now finish op:
        ch = oe-1;
        if ( str_r[oe] != '=' )	// '[<>]'
        {
          op_r = ( str_r[oe] == '<' ) ? Rel::LT : Rel::GT;
        }
        else
        { // '?='
          if ( ch != std::string::npos )
          {
            switch ( str_r[ch] )
            {
              case '<': --ch; op_r = Rel::LE; break;
              case '>': --ch; op_r = Rel::GE; break;
              case '!': --ch; op_r = Rel::NE; break;
              case '=': --ch; // fall through
              default:        op_r = Rel::EQ; break;
            }
          }
        }

        // finally name:
        if ( ch != std::string::npos ) // 'op' is not at str_r begin, so skip WS
          ch = backskipWs( str_r, ch );
        str_r.erase( ch+1 );
        return;
      }
      // HERE: It's a plain 'name'
    }

    /** Build \ref Capability from data. No parsing required.
    */
    sat::detail::IdType relFromStr( sat::detail::CPool * pool_r,
                                    const Arch & arch_r,
                                    const std::string & name_r,
                                    Rel op_r,
                                    const Edition & ed_r,
                                    const ResKind & kind_r )
    {
      // First build the name, non-packages prefixed by kind
      SplitIdent split( kind_r, name_r );
      sat::detail::IdType nid( split.ident().id() );

      if ( split.kind() == ResKind::srcpackage )
      {
        // map 'kind srcpackage' to 'arch src', the pseudo architecture
        // libsolv uses.
        nid = ::pool_rel2id( pool_r, nid, IdString(ARCH_SRC).id(), REL_ARCH, /*create*/true );
      }

      // Extend name by architecture, if provided and not a srcpackage
      if ( ! arch_r.empty() && kind_r != ResKind::srcpackage )
      {
        nid = ::pool_rel2id( pool_r, nid, arch_r.id(), REL_ARCH, /*create*/true );
      }

      // Extend 'op edition', if provided
      if ( op_r != Rel::ANY && ed_r != Edition::noedition )
      {
        nid = ::pool_rel2id( pool_r, nid, ed_r.id(), op_r.bits(), /*create*/true );
      }

      return nid;
    }

   /** Build \ref Capability from data, just parsing name for '[.arch]' and detect
    * 'kind srcpackage' (will be mapped to arch \c src).
    */
    sat::detail::IdType relFromStr( sat::detail::CPool * pool_r,
                                    const std::string & name_r, Rel op_r, const Edition & ed_r,
                                    const ResKind & kind_r )
    {
      static const Arch srcArch( IdString(ARCH_SRC).asString() );
      static const Arch nosrcArch( IdString(ARCH_NOSRC).asString() );
      static const std::string srcKindPrefix( ResKind::srcpackage.asString() + ':' );

      // check for an embedded 'srcpackage:foo' to be mapped to 'foo' and 'ResKind::srcpackage'.
      if ( kind_r.empty() && zypp::str::hasPrefix( name_r, srcKindPrefix ) )
      {
        return relFromStr( pool_r, Arch_empty, name_r.substr( srcKindPrefix.size() ), op_r, ed_r, ResKind::srcpackage );
      }

      Arch arch( Arch_empty );
      std::string name( name_r );

      std::string::size_type asep( name_r.rfind( '.' ) );
      if ( asep != std::string::npos )
      {
        Arch ext( name_r.substr( asep+1 ) );
        if ( ext.isBuiltIn() || ext == srcArch || ext == nosrcArch )
        {
          arch = ext;
          name.erase( asep );
        }
      }

      return relFromStr( pool_r, arch, name, op_r, ed_r, kind_r );
    }

    /** Full parse from string, unless Capability::PARSED.
    */
    sat::detail::IdType relFromStr( sat::detail::CPool * pool_r,
                                    const Arch & arch_r, // parse from name if empty
                                    const std::string & str_r, const ResKind & kind_r,
                                    Capability::CtorFlag flag_r )
    {
      std::string name( str_r );
      Rel         op;
      Edition     ed;
      if ( flag_r == Capability::UNPARSED )
      {
        splitOpEdition( name, op, ed );
      }

      if ( arch_r.empty() )
        return relFromStr( pool_r, name, op, ed, kind_r ); // parses for name[.arch]
      // else
      return relFromStr( pool_r, arch_r, name, op, ed, kind_r );
    }

    /** Parse richdeps from string, otherwise fall back to the traditional parser. */
    sat::detail::IdType richOrRelFromStr( sat::detail::CPool * pool_r, const std::string & str_r, const ResKind & prefix_r, Capability::CtorFlag flag_r )
    {
      if ( str_r[0] == '(' ) {
        sat::detail::IdType res { StringPool::instance().parserpmrichdep( str_r.c_str() ) };
        if ( res ) return res;
        // else: no richdep, so fall back to the ordinary parser which in
        // case of doubt turns the string into a NAMED cap.
      }
      return relFromStr( pool_r, Arch_empty, str_r, prefix_r, flag_r );
    }

    /////////////////////////////////////////////////////////////////
  } // namespace
  ///////////////////////////////////////////////////////////////////

  const Capability Capability::Null( STRID_NULL );
  const Capability Capability::Empty( STRID_EMPTY );

  /////////////////////////////////////////////////////////////////

  Capability::Capability( const char * str_r, const ResKind & prefix_r, CtorFlag flag_r )
  : _id( richOrRelFromStr( StringPool::instance().getPool(), str_r, prefix_r, flag_r ) )
  {}

  Capability::Capability( const std::string & str_r, const ResKind & prefix_r, CtorFlag flag_r )
  : _id( richOrRelFromStr( StringPool::instance().getPool(), str_r, prefix_r, flag_r ) )
  {}

  Capability::Capability( const Arch & arch_r, const char * str_r, const ResKind & prefix_r, CtorFlag flag_r )
  : _id( relFromStr( StringPool::instance().getPool(), arch_r, str_r, prefix_r, flag_r ) )
  {}

  Capability::Capability( const Arch & arch_r, const std::string & str_r, const ResKind & prefix_r, CtorFlag flag_r )
  : _id( relFromStr( StringPool::instance().getPool(), arch_r, str_r, prefix_r, flag_r ) )
  {}

  Capability::Capability( const char * str_r, CtorFlag flag_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), Arch_empty, str_r, prefix_r, flag_r ) )
  {}

  Capability::Capability( const std::string & str_r, CtorFlag flag_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), Arch_empty, str_r, prefix_r, flag_r ) )
  {}

  Capability::Capability( const Arch & arch_r, const char * str_r, CtorFlag flag_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), arch_r, str_r, prefix_r, flag_r ) )
  {}

  Capability::Capability( const Arch & arch_r, const std::string & str_r, CtorFlag flag_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), arch_r, str_r, prefix_r, flag_r ) )
  {}

  ///////////////////////////////////////////////////////////////////
  // Ctor from <name[.arch] op edition>.
  ///////////////////////////////////////////////////////////////////

  Capability::Capability( const std::string & name_r, const std::string & op_r, const std::string & ed_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), name_r, Rel(op_r), Edition(ed_r), prefix_r ) )
  {}
  Capability::Capability( const std::string & name_r, Rel op_r, const std::string & ed_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), name_r, op_r, Edition(ed_r), prefix_r ) )
  {}
  Capability::Capability( const std::string & name_r, Rel op_r, const Edition & ed_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), name_r, op_r, ed_r, prefix_r ) )
  {}

  ///////////////////////////////////////////////////////////////////
  // Ctor from <arch name op edition>.
  ///////////////////////////////////////////////////////////////////

  Capability::Capability( const std::string & arch_r, const std::string & name_r, const std::string & op_r, const std::string & ed_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), Arch(arch_r), name_r, Rel(op_r), Edition(ed_r), prefix_r ) )
  {}
  Capability::Capability( const std::string & arch_r, const std::string & name_r, Rel op_r, const std::string & ed_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), Arch(arch_r), name_r, op_r, Edition(ed_r), prefix_r ) )
  {}
  Capability::Capability( const std::string & arch_r, const std::string & name_r, Rel op_r, const Edition & ed_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), Arch(arch_r), name_r, op_r, ed_r, prefix_r ) )
  {}
  Capability::Capability( const Arch & arch_r, const std::string & name_r, const std::string & op_r, const std::string & ed_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), arch_r, name_r, Rel(op_r), Edition(ed_r), prefix_r ) )
  {}
  Capability::Capability( const Arch & arch_r, const std::string & name_r, Rel op_r, const std::string & ed_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), arch_r, name_r, op_r, Edition(ed_r), prefix_r ) )
  {}
  Capability::Capability( const Arch & arch_r, const std::string & name_r, Rel op_r, const Edition & ed_r, const ResKind & prefix_r )
  : _id( relFromStr( StringPool::instance().getPool(), arch_r, name_r, op_r, ed_r, prefix_r ) )
  {}

  Capability::Capability( Capability::CapRel rel_r, const Capability & lhs_r, const Capability & rhs_r )
  : _id( ::pool_rel2id( StringPool::instance().getPool(), lhs_r.id(), rhs_r.id(), rel_r, /*create*/true ) )
  {}

  ///////////////////////////////////////////////////////////////////
  // Ctor creating a namespace: capability.
  ///////////////////////////////////////////////////////////////////

  Capability::Capability( ResolverNamespace namespace_r, IdString value_r )
  : _id( ::pool_rel2id( StringPool::instance().getPool(), asIdString(namespace_r).id(), (value_r.empty() ? STRID_NULL : value_r.id() ), REL_NAMESPACE, /*create*/true ) )
  {}

  const char * Capability::c_str() const
  {
    return detail::cap2str( StringPool::instance().getPool(), id(), 0 );
  }

  CapMatch Capability::_doMatch( sat::detail::IdType lhs,  sat::detail::IdType rhs )
  {
    if ( lhs == rhs )
      return CapMatch::yes;

    CapDetail l( lhs );
    CapDetail r( rhs );

    switch ( l.kind() )
    {
      case CapDetail::NOCAP:
        return( r.kind() == CapDetail::NOCAP ); // NOCAP matches NOCAP only
        break;
      case CapDetail::EXPRESSION:
        return CapMatch::irrelevant;
        break;
      case CapDetail::NAMED:
      case CapDetail::VERSIONED:
        break;
    }

    switch ( r.kind() )
    {
      case CapDetail::NOCAP:
        return CapMatch::no; // match case handled above
        break;
      case CapDetail::EXPRESSION:
        return CapMatch::irrelevant;
        break;
      case CapDetail::NAMED:
      case CapDetail::VERSIONED:
        break;
    }
    // comparing two simple caps:
    if ( l.name() != r.name() )
      return CapMatch::no;

    // if both are arch restricted they must match
    if ( l.arch() != r.arch()
         && ! ( l.arch().empty() || r.arch().empty() ) )
      return CapMatch::no;

    // isNamed matches ANY edition:
    if ( l.isNamed() || r.isNamed() )
      return CapMatch::yes;

    // both are versioned:
    return overlaps( Edition::MatchRange( l.op(), l.ed() ),
                     Edition::MatchRange( r.op(), r.ed() ) );
  }

  bool Capability::isInterestingFileSpec( const char * name_r )
  {
    static       zypp::str::smatch what;
    static const zypp::str::regex  filenameRegex(
                 "/(s?bin|lib(64)?|etc)/|^/usr/(games/|share/(dict/words|magic\\.mime)$)|^/opt/gnome/games/",
                 zypp::str::regex::nosubs );

    return zypp::str::regex_match( name_r, what, filenameRegex );
  }

#if MOVE_CODE_FROM_LEGACY
  Capability Capability::guessPackageSpec( const std::string & str_r, bool & rewrote_r )
  {
    Capability cap( str_r );
    CapDetail detail( cap.detail() );

    // str_r might be the form "libzypp-1.2.3-4.5(.arch)'
    // correctly parsed as name capability by the ctor.
    // TODO: Think about allowing glob char in name - for now don't process
    if ( detail.isNamed() && !::strpbrk( detail.name().c_str(), "*?[{" )
      && ::strrchr( detail.name().c_str(), '-' ) && sat::WhatProvides( cap ).empty() )
    {
      Arch origArch( detail.arch() ); // to support a trailing .arch

      std::string guess( detail.name().asString() );
      std::string::size_type pos( guess.rfind( '-' ) );
      guess[pos] = '=';

      Capability guesscap( origArch, guess );
      detail = guesscap.detail();

      ResPool pool( ResPool::instance() );
      // require name part matching a pool items name (not just provides!)
      if ( pool.byIdentBegin( detail.name() ) != pool.byIdentEnd( detail.name() ) )
      {
        rewrote_r = true;
        return guesscap;
      }

      // try the one but last '-'
      if ( pos )
      {
        guess[pos] = '-';
        if ( (pos = guess.rfind( '-', pos-1 )) != std::string::npos )
        {
          guess[pos] = '=';

          guesscap = Capability( origArch, guess );
          detail = guesscap.detail();

          // require name part matching a pool items name (not just provides!)
          if ( pool.byIdentBegin( detail.name() ) != pool.byIdentEnd( detail.name() ) )
          {
            rewrote_r = true;
            return guesscap;
          }
        }
      }
    }

    rewrote_r = false;
    return cap;
  }

  Capability Capability::guessPackageSpec( const std::string & str_r )
  {
    bool dummy = false;
    return guessPackageSpec( str_r, dummy );
  }
#endif

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : CapDetail
  //
  ///////////////////////////////////////////////////////////////////

  void CapDetail::_init()
  {
    // : _kind( NOCAP ), _lhs( id_r ), _rhs( 0 ), _flag( 0 ), _archIfSimple( 0 )

    if ( _lhs == sat::detail::emptyId || _lhs == sat::detail::noId )
      return; // NOCAP

    if ( ! ISRELDEP(_lhs) )
    {
      // this is name without arch!
      _kind = NAMED;
      return;
    }

    ::Reldep * rd = GETRELDEP( StringPool::instance().getPool(), _lhs );
    _lhs  = rd->name;
    _rhs  = rd->evr;
    _flag = rd->flags;

    if ( Rel::isRel( _flag ) )
    {
      _kind = VERSIONED;
      // Check for name.arch...
      if ( ! ISRELDEP(_lhs) )
        return; // this is name without arch!
      rd = GETRELDEP( StringPool::instance().getPool(), _lhs );
      if ( rd->flags != CAP_ARCH )
        return; // this is not name.arch
      // This is name.arch:
      _lhs = rd->name;
      _archIfSimple = rd->evr;
    }
    else if ( rd->flags == CAP_ARCH )
    {
      _kind = NAMED;
      // This is name.arch:
      _lhs = rd->name;
      _archIfSimple = rd->evr;
    }
    else
    {
      _kind = EXPRESSION;
      return;
    }
    // map back libsolvs pseudo arch 'src' to kind srcpackage
    if ( _archIfSimple == ARCH_SRC || _archIfSimple == ARCH_NOSRC )
    {
      _lhs = IdString( (ResKind::srcpackage.asString() + ":" + IdString(_lhs).c_str()).c_str() ).id();
      _archIfSimple = 0;
    }
  }
} // namespace zyppng::sat
