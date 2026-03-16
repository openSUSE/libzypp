/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include <zypp/ng/sat/solvablespec.h>
#include <zypp/ng/sat/pool.h>
#include <zypp/ng/sat/solvable.h>
#include <zypp-core/base/IOStream.h>
#include <zypp-core/base/inputstream.h>
#include <zypp-core/base/String.h>

#include <vector>

extern "C" {
#include <solv/pool.h>
}

namespace zyppng::sat {

  bool SolvableSpec::contains( Pool & pool_r, const Solvable & solv_r ) const
  {
    if ( !solv_r || solv_r.isKind( ResKind::srcpackage ) )
      return false;

    // Fast path: ident match
    if ( !_idents.empty() && _idents.count( solv_r.ident() ) )
      return true;

    // Provides path: for each token, check the whatprovides list
    if ( !_provides.empty() ) {
      detail::CPool * cpool = pool_r.get();
      for ( const Capability & cap : _provides ) {
        // pool_whatprovides returns an offset into whatprovidesdata[].
        // The list is terminated by a 0 entry.
        unsigned offset = ::pool_whatprovides( cpool, cap.id() );
        const detail::IdType * p = cpool->whatprovidesdata + offset;
        for ( ; *p; ++p ) {
          if ( static_cast<detail::SolvableIdType>(*p) == solv_r.id() )
            return true;
        }
      }
    }

    return false;
  }

  void SolvableSpec::parse( std::string_view spec_r )
  {
    static constexpr std::string_view providesPrefix { "provides:" };
    if ( spec_r.empty() || spec_r[0] == '#' )
      return;
    if ( spec_r.substr( 0, providesPrefix.size() ) == providesPrefix )
      addProvides( Capability( std::string( spec_r.substr( providesPrefix.size() ) ).c_str() ) );
    else
      addIdent( IdString( std::string( spec_r ).c_str() ) );
  }

  void SolvableSpec::parseFrom( const zypp::InputStream & istr_r )
  {
    zypp::iostr::simpleParseFile( istr_r.stream(),
      [this]( int /*num_r*/, const std::string & line_r ) -> bool {
        parse( line_r );
        return true;
      });
  }

  void SolvableSpec::splitParseFrom( std::string_view multispec_r )
  {
    std::vector<std::string> tokens;
    zypp::str::splitEscaped( std::string( multispec_r ).c_str(),
                             std::back_inserter(tokens), ", \t" );
    parseFrom( tokens.begin(), tokens.end() );
  }

  // -------------------------------------------------------------------------
  // EvaluatedSolvableSpec
  // -------------------------------------------------------------------------

  EvaluatedSolvableSpec::EvaluatedSolvableSpec( Pool & pool_r, const SolvableSpec & spec_r )
  {
    if ( spec_r.empty() )
      return;

    detail::CPool * cpool = pool_r.get();

    // Ident-based: walk all solvables and check ident membership.
    if ( !spec_r.idents().empty() ) {
      for ( detail::SolvableIdType id = pool_r.getFirstId();
            id != detail::noSolvableId;
            id = pool_r.getNextId( id ) )
      {
        Solvable solv( id );
        if ( !solv.isKind( ResKind::srcpackage )
             && spec_r.idents().count( solv.ident() ) )
          _ids.insert( id );
      }
    }

    // Provides-based: use libsolv's whatprovides index.
    for ( const Capability & cap : spec_r.provides() ) {
      unsigned offset = ::pool_whatprovides( cpool, cap.id() );
      const detail::IdType * p = cpool->whatprovidesdata + offset;
      for ( ; *p; ++p ) {
        if ( !Solvable( static_cast<detail::SolvableIdType>(*p) ).isKind( ResKind::srcpackage ) )
          _ids.insert( static_cast<detail::SolvableIdType>( *p ) );
      }
    }
  }

  bool EvaluatedSolvableSpec::contains( const Solvable & solv_r ) const
  {
    if ( !solv_r || solv_r.isKind( ResKind::srcpackage ) )
      return false;
    return _ids.count( solv_r.id() ) != 0;
  }

} // namespace zyppng::sat
