#include <boost/program_options.hpp>

#include <iostream>
#include <fstream>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

#include <zypp/sat/SolvableSet.h>


#include <zypp/misc/DefaultLoadSystem.h>
// #include <zypp/sat/Pool.h>
#include <zypp/ResPool.h>
#include <zypp/PoolQuery.h>
#include <zypp/ResObjects.h>
#include <zypp/ui/SelectableTraits.h>


using std::cout;
using std::cerr;
using std::endl;
using std::flush;
#define MSG cerr << "*** "

// The bidirectionalS selector specifies that the graph will provide the in_edges() function
//  as well as the out_edges() function.This imposes twice as much space overhead per edge.
#define G_BIDIRECTIONAL 0

#if G_BIDIRECTIONAL
#define G_DIRECTIONAL_TAG bidirectionalS
#else
#define G_DIRECTIONAL_TAG bidirectionalS
#endif
namespace zypp::graph {
  namespace bg = boost; // graph functions are in boost::

  /// \brief Graph representing sat::Solvable relations
  ///
  /// A bunch of helpful graph-specific typedefs and methods were found at
  /// https://stackoverflow.com/questions/671714/modifying-vertex-properties-in-a-boostgraph
  class SolvGraph
  {
  public:
    using size_type               = unsigned;

    using VertexData              = sat::Solvable;  // not used as property because _vidx is maintained
#if G_BIDIRECTIONAL
#define G_DIRECTIONAL_TAG bidirectionalS
#else
#define G_DIRECTIONAL_TAG directedS
#endif
    using GraphContainer          = bg::adjacency_list<bg::setS, bg::vecS, bg::G_DIRECTIONAL_TAG>;
    using Vertex                  = typename bg::graph_traits<GraphContainer>::vertex_descriptor;
    using Edge                    = typename bg::graph_traits<GraphContainer>::edge_descriptor;

    using vertex_iter             = typename bg::graph_traits<GraphContainer>::vertex_iterator;
    using adjacent_vertex_iter    = typename bg::graph_traits<GraphContainer>::adjacency_iterator;
    using edge_iter               = typename bg::graph_traits<GraphContainer>::edge_iterator;
    using out_edge_iter           = typename bg::graph_traits<GraphContainer>::out_edge_iterator;
#if G_BIDIRECTIONAL
    using in_edge_iter            = typename bg::graph_traits<GraphContainer>::in_edge_iterator;
#endif

    using vertex_range_t          = std::pair<vertex_iter, vertex_iter>;
    using adjacent_vertex_range_t = std::pair<adjacent_vertex_iter, adjacent_vertex_iter>;
    using edge_range_t            = std::pair<edge_iter, edge_iter>;
    using out_edge_range_t        = std::pair<out_edge_iter, out_edge_iter>;
#if G_BIDIRECTIONAL
    using in_edge_range_t         = std::pair<in_edge_iter, in_edge_iter>;
#endif

  public:
    SolvGraph()
    {}

  public:
    // NOTE removing individual vertices/edges invalidates all descriptors if bg::vecS is used

    void clear()
    { _graph.clear(); _d2v.clear(); _v2d.clear(); }

    std::pair<Vertex,bool> addVertex( const VertexData& data )
    {
      if ( auto it = _d2v.find( data ); it != _d2v.end() )
        return { it->second, false };
      Vertex v = bg::add_vertex( _graph );
      _d2v[data] = v;
      _v2d[v] = data;
      return { v, true };
    }

    Edge addEdge( const VertexData& d1, const VertexData& d2 ) { return addEdge( vertex(d1), vertex(d2) ); }
    Edge addEdge( const VertexData& d1, const Vertex& v2 )     { return addEdge( vertex(d1), v2 ); }
    Edge addEdge( const Vertex& v1, const VertexData& d2 )     { return addEdge( v1, vertex(d2) ); }
    Edge addEdge( const Vertex& v1, const Vertex& v2 )
    {
      /* TODO: maybe one wants to check if this edge could be inserted */
      Edge e = bg::add_edge( v1, v2, _graph ).first;
      return e;
    }

  public:
    const GraphContainer& graph() const
    { return _graph; }


    size_type vertexCount() const
    { return bg::num_vertices( _graph ); }

    vertex_range_t vertices() const
    { return bg::vertices( _graph ); }

    bool hasVertex( const VertexData& data ) const
    { return _d2v.find( data ) != _d2v.end(); }

    Vertex vertex( const VertexData& data )
    { return addVertex( data ).first; }

    const VertexData& vertexData( const Vertex& v ) const
    { return _v2d.at(v); }

    size_type vertexOutDegree( const Vertex& v ) const
    { return bg::out_degree( v, _graph ); }

#if G_BIDIRECTIONAL
    size_type vertexInDegree( const Vertex& v ) const
    { return bg::in_degree( v, _graph ); }
#endif

    adjacent_vertex_range_t adjacentVertices( const Vertex& v ) const
    { return bg::adjacent_vertices( v, _graph ); }


    Vertex source( const Edge& e ) const
    { return bg::source( e, _graph ); }

    const VertexData& sourceData( const Edge& e ) const
    { return vertexData( source(e) ); }

    Vertex target( const Edge& e ) const
    { return bg::target( e, _graph ); }

    const VertexData& targetData( const Edge& e ) const
    { return vertexData( target(e) ); }

  private:
    GraphContainer _graph;
    std::map<VertexData,Vertex> _d2v; // BiMap to enforce unique VertexData vertices
    std::map<Vertex,VertexData> _v2d; //
  };


  /// \brief graphviz vertex (AKA sat::Solvable) property writer
  template <class SolvGraph>
  struct VertexPropertyWriter
  {
    template <class VertexOrEdge>
    void operator()( std::ostream& out, const VertexOrEdge& v ) const {
      const typename SolvGraph::VertexData& slv { _graph.vertexData(v) };
      if ( slv )
        out << "[label=\""<< slv.ident() <<"\\n"<< slv.edition() <<"\\n"<< slv.arch() <<"\\n"<< slv.repository().alias() <<"\"]";
      else
        out << "[label=\"noSolvable\" color=\"red\"]";
    }

    VertexPropertyWriter( const SolvGraph & graph )
    : _graph { graph }
    {}
  protected:
    const SolvGraph & _graph;
  };

  /// \brief graphviz edge property writer
  template <class SolvGraph>
  struct EdgePropertyWriter
  {
    template <class VertexOrEdge>
    void operator()( std::ostream& out, const VertexOrEdge& e ) const {
      if ( !_graph.targetData( e ) )
        out << "[color=\"red\"]";
    }

    EdgePropertyWriter( const SolvGraph & graph )
    : _graph { graph }
    {}
  protected:
    const SolvGraph & _graph;
  };

  /// \brief graphviz writer to std::ostream
  template <class SolvGraph>
  std::ostream & write_graphviz( std::ostream & str, const SolvGraph & graph )
  {
    bg::write_graphviz( str, graph.graph(), VertexPropertyWriter(graph), EdgePropertyWriter(graph) );
    return str;

  }

} // namespace zypp::graph


int main()
{
  using namespace zypp;
  using namespace zypp::graph;

  misc::defaultLoadSystem( misc::LS_NOREFRESH|misc::LS_NOREPOS );
  sat::Pool satpool( sat::Pool::instance() );

  SolvGraph g;

  std::set<sat::Solvable> argSet;
  argSet.insert( ResPool::instance().proxy().lookup( IdString("zypper") )->installedObj().satSolvable() );
  for ( const sat::Solvable & solv : argSet ) {
    g.addVertex( solv );
  }

  std::set<sat::Solvable> addedSet( argSet );
  while ( not addedSet.empty() ) {
    std::set<sat::Solvable> todo;
    todo.swap( addedSet );
    for ( const sat::Solvable & solv : todo ) {
      MSG << "TODO " << solv << endl;
      std::set<sat::Solvable> targets;
      for ( const Capability & cap : solv.requires() ) {
        MSG << "? " << cap << endl;
        for ( const sat::Solvable & prv : sat::WhatProvides( cap ) ) {
          if ( not prv )
            continue;   // e.g. rpmlib(...) requires provided by the systemSolvable
          MSG << " - " << prv << endl;
          targets.insert( prv );
        }
      }
      for ( const sat::Solvable & trg : targets ) {
        if ( g.addVertex( trg ).second )
          addedSet.insert( trg ); // new vertex
        g.addEdge( solv, trg );
      }
    }

  }



//   g.addVertex( sat::Solvable(13) );
//   g.addVertex( sat::Solvable(13) );
//   g.addVertex( sat::Solvable(0) );
//   g.addEdge( sat::Solvable(13), sat::Solvable(14) );
//   g.addEdge( sat::Solvable(13), sat::Solvable(14) );
//   g.addEdge( sat::Solvable(14), sat::Solvable(0) );
//   g.addEdge( sat::Solvable(0), sat::Solvable(13) );


  // represent graph in DOT format and send to cout
  write_graphviz( cout, g );
  return 0;
}
