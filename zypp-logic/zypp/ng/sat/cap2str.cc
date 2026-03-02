/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "cap2str.h"
#include <zypp/ng/idstring.h>
#include <zypp/ng/reskind.h>
#include <zypp/ng/sat/capability.h>

namespace zyppng::sat::detail {

  // https://rpm-software-management.github.io/rpm/manual/boolean_dependencies.html
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

    inline bool isBoolOp( int op_r )
    {
      switch ( op_r ) {
        case REL_AND:
        case REL_OR:
        case REL_COND:
        case REL_UNLESS:
        case REL_ELSE:
        case REL_WITH:
        case REL_WITHOUT:
          return true;
      }
      return false;
    }

    inline bool needsBrace( int op_r, int parop_r )
    {
      return ( isBoolOp( parop_r ) || parop_r == 0 ) && isBoolOp( op_r )
          && ( parop_r != op_r || op_r == REL_COND || op_r == REL_UNLESS || op_r == REL_ELSE )
          && not ( ( parop_r == REL_COND || parop_r == REL_UNLESS ) && op_r == REL_ELSE );
    }
  }

  void cap2str( std::string & outs_r, CPool * pool_r, IdType id_r, int parop_r )
  {
    if ( ISRELDEP(id_r) ) {
      ::Reldep * rd = GETRELDEP( pool_r, id_r );
      int op = rd->flags;

      if ( op == CapDetail::CAP_ARCH ) {
        if ( rd->evr == ARCH_SRC || rd->evr == ARCH_NOSRC ) {
          // map arch .{src,nosrc} to kind srcpackage
          outs_r += ResKind::srcpackage.c_str();
          outs_r += ":";
          outs_r += IdString(rd->name).c_str();
          return;
        }
        cap2str( outs_r, pool_r, rd->name, op );
        outs_r += ".";
        cap2str( outs_r, pool_r, rd->evr, op );
        return;
      }

      if ( op == CapDetail::CAP_NAMESPACE ) {
        cap2str( outs_r, pool_r, rd->name, op );
        outs_r += "(";
        cap2str( outs_r, pool_r, rd->evr, op );
        outs_r += ")";
        return;
      }

      if ( op == REL_FILECONFLICT )
      {
        cap2str( outs_r, pool_r, rd->name, op );
        return;
      }

      if ( needsBrace( op, parop_r ) ) {
        outs_r += "(";
        cap2str( outs_r, pool_r, rd->name, op );
        outs_r += capRel2Str( op );
        cap2str( outs_r, pool_r, rd->evr, op );
        outs_r += ")";
        return;
      }

      cap2str( outs_r, pool_r, rd->name, op );
      outs_r += capRel2Str( op );
      cap2str( outs_r, pool_r, rd->evr, op );
    }
    else
      outs_r += IdString(id_r).c_str();
  }

  const char *cap2str( CPool *pool_r, IdType id_r, int parop_r )
  {
    if ( not id_r ) return "";
    if ( not ISRELDEP( id_r ) ) return IdString( id_r ).c_str();

    static TempStrings<5> tempstrs;   // Round Robin buffer to prolong the lifetime of the returned char*

    std::string & outs { tempstrs.getNext() };
    cap2str( outs, pool_r, id_r, 0 );
    return outs.c_str();
  }

  const char * capRel2Str( int op_r )
  {
    switch ( op_r )
    {
      case REL_GT:               return " > ";
      case REL_EQ:               return " = ";
      case REL_LT:               return " < ";
      case REL_GT|REL_EQ:        return " >= ";
      case REL_LT|REL_EQ:        return " <= ";
      case REL_GT|REL_LT:        return " != ";
      case REL_GT|REL_LT|REL_EQ: return " <=> ";
      case REL_AND:              return " and ";
      case REL_OR:               return " or ";
      case REL_COND:             return " if ";
      case REL_UNLESS:           return " unless ";
      case REL_ELSE:             return " else ";
      case REL_WITH:             return " with ";
      case REL_WITHOUT:          return " without ";
    }
    return "UNKNOWNCAPREL";
  }


} // namespace zyppng::sat::detail
