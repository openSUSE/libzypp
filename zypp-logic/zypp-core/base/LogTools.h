/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/LogTools.h
 *
*/
#ifndef ZYPP_BASE_LOGTOOLS_H
#define ZYPP_BASE_LOGTOOLS_H

#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>

#include <zypp-core/base/Hash.h>
#include <zypp-core/base/Logger.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/Iterator.h>
#include <zypp-core/Globals.h>

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>
#endif

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  /** Provide print[f] and sprint[f] functions based on JoinFormat.
   *
   * The default format writes all arguments ' '-separated to a stream or
   * std::string.
   */
  //@{
  namespace str {

    namespace detail {

      /** Helper to store a reference or move rvalues inside. */
      template <typename T> class RefStore;

      /** Move rvalues inside. */
      template <typename T>
      struct RefStore
      {
        constexpr RefStore( T && val_r )
        : _val { std::move(val_r) }
        {}

        RefStore( const RefStore & ) = delete;

        constexpr RefStore( RefStore && rhs )
        : _val { std::move(rhs._val) }
        {}

        T &       get()       { return _val; }
        const T & get() const { return _val; }

      private:
        T _val;
      };

      /** Store reference */
      template <typename T>
      struct RefStore<T&>
      {
        constexpr RefStore( T & t )
        : _val { t }
        {}

        RefStore( const RefStore & ) = delete;

        constexpr RefStore( RefStore && rhs )
        : _val { rhs._val }
        {}

        T &       get()       { return _val; }
        const T & get() const { return _val; }

      private:
        T & _val;
      };

      /** \relates RefStore<T> Create a RefStore for the argument. */
      template <typename T>
      constexpr auto makeRefStore( T && t ) ->/* c++-11 can not deduce return value! */ RefStore<T>
      { return RefStore<T>( std::forward<T>(t) ); }

      /** \relates RefStore<T> Stream output */
      template <typename T>
      std::ostream & operator<<( std::ostream & str, const RefStore<T> & obj )
      { return str << obj.get(); }


      /** Store nothing print nothing. */
      struct NoPrint {};

      inline std::ostream & operator<<( std::ostream & str, const NoPrint & obj )
      { return str; }

      template <>
      struct RefStore<NoPrint>
      { constexpr RefStore() {} constexpr RefStore( NoPrint && ) {} };

      template <>
      inline std::ostream & operator<<( std::ostream & str, const RefStore<NoPrint> & obj )
      { return str; }

      template <>
      struct RefStore<NoPrint&>
      { constexpr RefStore() {} constexpr RefStore( const NoPrint & ) {} };

      template <>
      inline std::ostream & operator<<( std::ostream & str, const RefStore<NoPrint&> & obj )
      { return str; }


      /** A basic format description to print a collection.
       *
       * The JoinFormat stores references or rvalues of printable objects
       * which define the format elements when printing a collection:
       * \code
       *  ELEMENT = PEL el SEL
       *  FORMAT  = INTRO [ PFX ELEMENT [ SEP ELEMENT ]* SFX ] EXTRO
       * \endcode
       *
       * \ref \see makeJoinFormat
       */
      template <typename Intro, typename Pfx, typename Sep, typename Sfx, typename Extro, typename Pel, typename Sel>
      struct JoinFormat
      {
        constexpr JoinFormat( Intro && intro, Pfx && pfx, Sep && sep, Sfx && sfx, Extro && extro, Pel && pel, Sel && sel )
        : _intro { std::forward<Intro>(intro) }
        , _pfx   { std::forward<Pfx>(pfx) }
        , _sep   { std::forward<Sep>(sep) }
        , _sfx   { std::forward<Sfx>(sfx) }
        , _extro { std::forward<Extro>(extro) }
        , _pel   { std::forward<Pel>(pel) }
        , _sel   { std::forward<Sel>(sel) }
        {}

        RefStore<Intro> _intro;
        RefStore<Pfx>   _pfx;
        RefStore<Sep>   _sep;
        RefStore<Sfx>   _sfx;
        RefStore<Extro> _extro;
        RefStore<Pel>   _pel;
        RefStore<Sel>   _sel;
      };

    } //namespace detail

    // Drag it into str:: namespace
    using NoPrint = detail::NoPrint;
    inline constexpr NoPrint noPrint;

    /** \relates JoinFormat<> Create a basic format description to print a collection.
     *
     * The JoinFormat stores references or rvalues of printable objects which
     * serve the following purpose when printing a collection:
     * \code
     *  ELEMENT = PEL el SEL
     *  FORMAT  = INTRO [ PFX ELEMENT [ SEP ELEMENT ]* SFX ] EXTRO
     * \endcode
     *  INTRO   : printed unconditionally at the beginning
     *  PFX     : printed before the 1st ELEMENT if the collection is not empty
     *  SEP     : printed between two ELEMENTs
     *  SFX     : printed after the last ELEMENT if the collection is not empty
     *  EXTRO   : printed unconditionally at the end
     *  PEL,SEL : Optionally every element may be enclosed by PEL and SEL (default \ref NoPrint)
     * \code
     * // space separated
     * makeJoinFormat( "", "", " ", "", "");
     * // space separated line (trailing NL)
     * makeJoinFormat( "", "", " ", "", "\n");
     * // tuple (singleline)
     * makeJoinFormat( "(", "", ", ", "", ")");
     * // set (multiline, indent 2)
     * makeJoinFormat( "{", "\n  ", ",\n  ", "\n", "}" );
     *
     * makeJoinFormat( "", "", "", "", "");
     *
     * \endcode
     */
    template <typename Intro, typename Pfx, typename Sep, typename Sfx, typename Extro, typename Pel=NoPrint, typename Sel=NoPrint>
    constexpr auto makeJoinFormat( Intro && intro, Pfx && pfx, Sep && sep, Sfx && sfx, Extro && extro, Pel && pel = Pel(), Sel && sel = Sel() ) ->/* c++-11 can not deduce return value! */ detail::JoinFormat<Intro,Pfx,Sep,Sfx,Extro,Pel,Sel>
    { return detail::JoinFormat<Intro,Pfx,Sep,Sfx,Extro,Pel,Sel>( std::forward<Intro>(intro), std::forward<Pfx>(pfx), std::forward<Sep>(sep), std::forward<Sfx>(sfx), std::forward<Extro>(extro), std::forward<Pel>(pel), std::forward<Sel>(sel) ); }


    // A few default formats:
    /** Concatenated. */
    inline constexpr auto FormatConcat = makeJoinFormat( noPrint, noPrint, noPrint, noPrint, noPrint );
    /** ' '-separated. */
    inline constexpr auto FormatWords  = makeJoinFormat( noPrint, noPrint,     " ", noPrint, noPrint );
    /** ' '-separated and NL-terminated! */
    inline constexpr auto FormatLine   = makeJoinFormat( noPrint, noPrint,     " ", noPrint,    "\n" );
    /** One item per line NL-terminated! */
    inline constexpr auto FormatList   = makeJoinFormat( noPrint, noPrint,    "\n",    "\n", noPrint );
    /** Tuple: (el, .., el). */
    inline constexpr auto FormatTuple  = makeJoinFormat(     "(", noPrint,    ", ", noPrint,     ")" );

    /** dumpRange default format: {}-enclosed and indented one item per line. */
    inline constexpr auto FormatDumpRangeDefault
                                       = makeJoinFormat(     "{",  "\n  ",  "\n  ",    "\n",     "}" );

    namespace detail {

      template <typename Ostream, typename Format>
      void _joinSF( Ostream & str, const Format & fmt )
      { ; }

      template <typename Ostream, typename Format, typename First>
      void _joinSF( Ostream & str, const Format & fmt, First && first )
      { str << fmt._pel << std::forward<First>(first) << fmt._sel; }

      template <typename Ostream, typename Format, typename First, typename... Args>
      void _joinSF( Ostream & str, const Format & fmt, First && first, Args &&... args )
      { _joinSF( str << fmt._pel << std::forward<First>(first) << fmt._sel << fmt._sep, fmt, std::forward<Args>(args)... ); }

      /** Print args on ostreamlike \a str using JoinFormat \a fmt.
       * \see \ref makeJoinFormat
       */
      template <typename Ostream, typename Format, typename... Args>
      Ostream & joinSF( Ostream & str, Format && fmt, Args &&... args )
      {
        str << fmt._intro;
        if ( sizeof...(Args) ) {
          str << fmt._pfx;
          detail::_joinSF( str, fmt, std::forward<Args>(args)... );
          str << fmt._sfx;
        }
        str << fmt._extro;
        return str;
      }

    } //namespace detail

    /** Print Format on stream */
    template <typename Ostream, typename Format, typename... Args>
    Ostream & printf( Ostream & str, Format && fmt, Args&&... args )
    { return detail::joinSF( str, std::forward<Format>(fmt), std::forward<Args>(args)... ); }

    /** Print Format fs string */
    template <typename Format, typename... Args>
    std::string sprintf( Format && fmt, Args&&... args )
    { str::Str str; return detail::joinSF( str, std::forward<Format>(fmt), std::forward<Args>(args)... ); }


    /** Print words on stream */
    template <typename Ostream, typename... Args>
    Ostream & print( Ostream & str, Args&&... args )
    { return detail::joinSF( str, FormatWords, std::forward<Args>(args)... ); }

    /** Print words as string */
    template <typename... Args>
    std::string sprint( Args&&... args )
    { str::Str str; return detail::joinSF( str, FormatWords, std::forward<Args>(args)... ); }

    /** Concat words on stream */
    template <typename Ostream, typename... Args>
    Ostream & concat( Ostream & str, Args&&... args )
    { return detail::joinSF( str, FormatConcat, std::forward<Args>(args)... ); }

    /** Concat words as string */
    template <typename... Args>
    std::string sconcat( Args&&... args )
    { str::Str str; return detail::joinSF( str, FormatConcat, std::forward<Args>(args)... ); }

    namespace detail {
      /** Log helper wrapping the Ostream and Format.
       * Aiming for pXXX( args... ) writing to log stream XXX.
       * Getting the logstream remembers file, function and line,
       * so we need #defines to get the stream at the code location
       * writing the log line.
       */
      template <typename Ostream, typename Format>
      struct PrintFmt {
        PrintFmt( Ostream & str, const Format & fmt )
        : _str { str }
        , _fmt { fmt }
        {}

        template <typename... Args>
        Ostream & operator()( Args&&... args )
        { return str::printf( _str, _fmt, std::forward<Args>(args)... ); }

      private:
        Ostream & _str;
        const Format & _fmt;
      };
    } //namespace detail

    #define pXXX zypp::str::detail::PrintFmt(XXX,zypp::str::FormatLine)
    #define pDBG zypp::str::detail::PrintFmt(DBG,zypp::str::FormatLine)
    #define pMIL zypp::str::detail::PrintFmt(MIL,zypp::str::FormatLine)
    #define pWAR zypp::str::detail::PrintFmt(WAR,zypp::str::FormatLine)
    #define pERR zypp::str::detail::PrintFmt(ERR,zypp::str::FormatLine)
    #define pSEC zypp::str::detail::PrintFmt(SEC,zypp::str::FormatLine)
    #define pINT zypp::str::detail::PrintFmt(INT,zypp::str::FormatLine)
    #define pUSR zypp::str::detail::PrintFmt(USR,zypp::str::FormatLine)

    #define wXXX zypp::str::detail::PrintFmt(XXX,zypp::str::FormatWords)
    #define wDBG zypp::str::detail::PrintFmt(DBG,zypp::str::FormatWords)
    #define wMIL zypp::str::detail::PrintFmt(MIL,zypp::str::FormatWords)
    #define wWAR zypp::str::detail::PrintFmt(WAR,zypp::str::FormatWords)
    #define wERR zypp::str::detail::PrintFmt(ERR,zypp::str::FormatWords)
    #define wSEC zypp::str::detail::PrintFmt(SEC,zypp::str::FormatWords)
    #define wINT zypp::str::detail::PrintFmt(INT,zypp::str::FormatWords)
    #define wUSR zypp::str::detail::PrintFmt(USR,zypp::str::FormatWords)
  } // namespace str
  //@}

  using std::endl;

  /// \brief Helper to produce not-NL-terminated multi line output.
  /// Used as leading separator it prints a separating NL by omitting
  /// output upon it's first invocation.
  /// A custom separator char can be passed to the ctor.
  /// \code
  ///   Container foo { 1,2,3 };
  ///   MLSep sep;
  ///   for ( auto && el : foo )
  ///     cout << sep << el;
  ///   # "1\n2\n3"
  /// \endcode
  struct MLSep
  {
    MLSep() {}
    MLSep( char sep_r ) : _sep { sep_r } {}
    bool _first = true;
    char _sep = '\n';
  };
  inline std::ostream & operator<<( std::ostream & str, MLSep & obj )
  { if ( obj._first ) obj._first = false; else str << obj._sep; return str; }

  /** Print range defined by iterators (multiline style).
   * \code
   * intro [ pfx ITEM [ { sep ITEM }+ ] sfx ] extro
   * \endcode
   *
   * The defaults print the range enclosed in \c {}, one item per
   * line indented by 2 spaces.
   * \code
   * {
   *   item1
   *   item2
   * }
   * {} // on empty range
   * \endcode
   *
   * A comma separated list enclosed in \c () would be:
   * \code
   * dumpRange( stream, begin, end, "(", "", ", ", "", ")" );
   * // or shorter:
   * dumpRangeLine( stream, begin, end );
   * \endcode
   *
   * \note Some special handling is required for printing std::maps.
   * Therefore iomaipulators \ref dumpMap, \ref dumpKeys and \ref dumpValues
   * are provided.
   * \code
   * std::map<string,int> m;
   * m["a"]=1;
   * m["b"]=2;
   * m["c"]=3;
   *
   * dumpRange( DBG, dumpMap(m).begin(), dumpMap(m).end() ) << endl;
   * // {
   * //   [a] = 1
   * //   [b] = 2
   * //   [c] = 3
   * // }
   * dumpRange( DBG, dumpKeys(m).begin(), dumpKeys(m).end() ) << endl;
   * // {
   * //   a
   * //   b
   * //   c
   * // }
   * dumpRange( DBG, dumpValues(m).begin(), dumpValues(m).end() ) << endl;
   * // {
   * //   1
   * //   2
   * //   3
   * // }
   * dumpRangeLine( DBG, dumpMap(m).begin(), dumpMap(m).end() ) << endl;
   * // ([a] = 1, [b] = 2, [c] = 3)
   * dumpRangeLine( DBG, dumpKeys(m).begin(), dumpKeys(m).end() ) << endl;
   * // (a, b, c)
   * dumpRangeLine( DBG, dumpValues(m).begin(), dumpValues(m).end() ) << endl;
   * // (1, 2, 3)
   * \endcode
  */
  template<class TIterator>
    std::ostream & dumpRange( std::ostream & str,
                              TIterator begin, TIterator end,
                              const std::string & intro = "{",
                              const std::string & pfx   = "\n  ",
                              const std::string & sep   = "\n  ",
                              const std::string & sfx   = "\n",
                              const std::string & extro = "}" )
    {
      str << intro;
      if ( begin != end )
        {
          str << pfx << *begin;
          for (  ++begin; begin != end; ++begin )
            str << sep << *begin;
          str << sfx;
        }
      return str << extro;
    }

  /** Print range defined by iterators (single line style).
   * \see dumpRange
   */
  template<class TIterator>
    std::ostream & dumpRangeLine( std::ostream & str,
                                  TIterator begin, TIterator end )
    { return dumpRange( str, begin, end, "(", "", ", ", "", ")" ); }
  /** \overload for container */
  template<class TContainer>
    std::ostream & dumpRangeLine( std::ostream & str, const TContainer & cont )
    { return dumpRangeLine( str, cont.begin(), cont.end() ); }


  ///////////////////////////////////////////////////////////////////
  namespace iomanip
  {
    ///////////////////////////////////////////////////////////////////
    /// \class RangeLine<TIterator>
    /// \brief Iomanip helper printing dumpRangeLine style
    ///////////////////////////////////////////////////////////////////
    template<class TIterator>
    struct RangeLine
    {
      RangeLine( TIterator begin, TIterator end )
      : _begin( begin )
      , _end( end )
      {}
      TIterator _begin;
      TIterator _end;
    };

    /** \relates RangeLine<TIterator> */
    template<class TIterator>
    std::ostream & operator<<( std::ostream & str, const RangeLine<TIterator> & obj )
    { return dumpRangeLine( str, obj._begin, obj._end ); }

  } // namespce iomanip
  ///////////////////////////////////////////////////////////////////

  /** Iomanip printing dumpRangeLine style
   * \code
   *   std::vector<int> c( { 1, 1, 2, 3, 5, 8 } );
   *   std::cout << rangeLine(c) << std::endl;
   *   -> (1, 1, 2, 3, 5, 8)
   * \endcode
   */
  template<class TIterator>
  iomanip::RangeLine<TIterator> rangeLine( TIterator begin, TIterator end )
  { return iomanip::RangeLine<TIterator>( begin, end ); }
  /** \overload for container */
  template<class TContainer>
  auto rangeLine( const TContainer & cont ) -> decltype( rangeLine( cont.begin(), cont.end() ) )
  { return rangeLine( cont.begin(), cont.end() ); }

} // namespace zypp
///////////////////////////////////////////////////////////////////
namespace std {
  template<class Tp>
    std::ostream & operator<<( std::ostream & str, const std::vector<Tp> & obj )
    { return zypp::dumpRange( str, obj.begin(), obj.end() ); }

  template<class Tp, class TCmp, class TAlloc>
    std::ostream & operator<<( std::ostream & str, const std::set<Tp,TCmp,TAlloc> & obj )
    { return zypp::dumpRange( str, obj.begin(), obj.end() ); }

  template<class Tp>
    std::ostream & operator<<( std::ostream & str, const std::unordered_set<Tp> & obj )
    { return zypp::dumpRange( str, obj.begin(), obj.end() ); }

  template<class Tp>
    std::ostream & operator<<( std::ostream & str, const std::multiset<Tp> & obj )
    { return zypp::dumpRange( str, obj.begin(), obj.end() ); }

  template<class Tp>
    std::ostream & operator<<( std::ostream & str, const std::list<Tp> & obj )
    { return zypp::dumpRange( str, obj.begin(), obj.end() ); }
} // namespace std
///////////////////////////////////////////////////////////////////
namespace zypp {

  template<class Tp>
    std::ostream & operator<<( std::ostream & str, const Iterable<Tp> & obj )
    { return dumpRange( str, obj.begin(), obj.end() ); }

  ///////////////////////////////////////////////////////////////////
  namespace _logtoolsdetail
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    // mapEntry
    ///////////////////////////////////////////////////////////////////

    /** std::pair wrapper for std::map output.
     * Just because we want a special output format for std::pair
     * used in a std::map. The mapped std::pair is printed as
     * <tt>[key] = value</tt>.
    */
    template<class TPair>
      class MapEntry
      {
      public:
        MapEntry( const TPair & pair_r )
        : _pair( &pair_r )
        {}

        const TPair & pair() const
        { return *_pair; }

      private:
        const TPair *const _pair;
      };

    /** \relates MapEntry Stream output. */
    template<class TPair>
      std::ostream & operator<<( std::ostream & str, const MapEntry<TPair> & obj )
      {
        return str << '[' << obj.pair().first << "] = " << obj.pair().second;
      }

    /** \relates MapEntry Convenience function to create MapEntry from std::pair. */
    template<class TPair>
      MapEntry<TPair> mapEntry( const TPair & pair_r )
      { return MapEntry<TPair>( pair_r ); }

    ///////////////////////////////////////////////////////////////////
    // dumpMap
    ///////////////////////////////////////////////////////////////////

    /** std::map wrapper for stream output.
     * Uses a transform_iterator to wrap the std::pair into MapEntry.
     *
     */
    template<class TMap>
      class DumpMap
      {
      public:
        using MapType = TMap;
        using PairType = typename TMap::value_type;
        using MapEntryType = MapEntry<PairType>;

        struct Transformer
        {
          MapEntryType operator()( const PairType & pair_r ) const
          { return mapEntry( pair_r ); }
        };

        using MapEntry_const_iterator = transform_iterator<Transformer, typename MapType::const_iterator>;

      public:
        DumpMap( const TMap & map_r )
        : _map( &map_r )
        {}

        const TMap & map() const
        { return *_map; }

        MapEntry_const_iterator begin() const
        { return make_transform_iterator( map().begin(), Transformer() ); }

        MapEntry_const_iterator end() const
        { return make_transform_iterator( map().end(), Transformer() );}

      private:
        const TMap *const _map;
      };

    /** \relates DumpMap Stream output. */
    template<class TMap>
      std::ostream & operator<<( std::ostream & str, const DumpMap<TMap> & obj )
      { return dumpRange( str, obj.begin(), obj.end() ); }

    /** \relates DumpMap Convenience function to create DumpMap from std::map. */
    template<class TMap>
      DumpMap<TMap> dumpMap( const TMap & map_r )
      { return DumpMap<TMap>( map_r ); }

    ///////////////////////////////////////////////////////////////////
    // dumpKeys
    ///////////////////////////////////////////////////////////////////

    /** std::map wrapper for stream output of keys.
     * Uses MapKVIterator iterate and write the key values.
     * \code
     * std::map<...> mymap;
     * std::cout << dumpKeys(mymap) << std::endl;
     * \endcode
     */
    template<class TMap>
      class DumpKeys
      {
      public:
        using MapKey_const_iterator = typename MapKVIteratorTraits<TMap>::Key_const_iterator;

      public:
        DumpKeys( const TMap & map_r )
        : _map( &map_r )
        {}

        const TMap & map() const
        { return *_map; }

        MapKey_const_iterator begin() const
        { return make_map_key_begin( map() ); }

        MapKey_const_iterator end() const
        { return make_map_key_end( map() ); }

      private:
        const TMap *const _map;
      };

    /** \relates DumpKeys Stream output. */
    template<class TMap>
      std::ostream & operator<<( std::ostream & str, const DumpKeys<TMap> & obj )
      { return dumpRange( str, obj.begin(), obj.end() ); }

    /** \relates DumpKeys Convenience function to create DumpKeys from std::map. */
    template<class TMap>
      DumpKeys<TMap> dumpKeys( const TMap & map_r )
      { return DumpKeys<TMap>( map_r ); }

    ///////////////////////////////////////////////////////////////////
    // dumpValues
    ///////////////////////////////////////////////////////////////////

    /** std::map wrapper for stream output of values.
     * Uses MapKVIterator iterate and write the values.
     * \code
     * std::map<...> mymap;
     * std::cout << dumpValues(mymap) << std::endl;
     * \endcode
     */
    template<class TMap>
      class DumpValues
      {
      public:
        using MapValue_const_iterator = typename MapKVIteratorTraits<TMap>::Value_const_iterator;

      public:
        DumpValues( const TMap & map_r )
        : _map( &map_r )
        {}

        const TMap & map() const
        { return *_map; }

        MapValue_const_iterator begin() const
        { return make_map_value_begin( map() ); }

        MapValue_const_iterator end() const
        { return make_map_value_end( map() ); }

      private:
        const TMap *const _map;
      };

    /** \relates DumpValues Stream output. */
    template<class TMap>
      std::ostream & operator<<( std::ostream & str, const DumpValues<TMap> & obj )
      { return dumpRange( str, obj.begin(), obj.end() ); }

    /** \relates DumpValues Convenience function to create DumpValues from std::map. */
    template<class TMap>
      DumpValues<TMap> dumpValues( const TMap & map_r )
      { return DumpValues<TMap>( map_r ); }

    /////////////////////////////////////////////////////////////////
  } // namespace _logtoolsdetail
  ///////////////////////////////////////////////////////////////////

  // iomanipulator
  using _logtoolsdetail::mapEntry;   // std::pair as '[key] = value'
  using _logtoolsdetail::dumpMap;    // dumpRange '[key] = value'
  using _logtoolsdetail::dumpKeys;   // dumpRange keys
  using _logtoolsdetail::dumpValues; // dumpRange values

} // namespace zypp
///////////////////////////////////////////////////////////////////
namespace std {
  template<class TKey, class Tp>
    std::ostream & operator<<( std::ostream & str, const std::map<TKey, Tp> & obj )
    { return str << zypp::dumpMap( obj ); }

  template<class TKey, class Tp>
    std::ostream & operator<<( std::ostream & str, const std::unordered_map<TKey, Tp> & obj )
    { return str << zypp::dumpMap( obj ); }

  template<class TKey, class Tp>
    std::ostream & operator<<( std::ostream & str, const std::multimap<TKey, Tp> & obj )
    { return str << zypp::dumpMap( obj ); }

  /** Print stream status bits.
   * Prints the values of a streams \c good, \c eof, \c failed and \c bad bit.
   *
   * \code
   *  [g___] - good
   *  [_eF_] - eof and fail bit set
   *  [__FB] - fail and bad bit set
   * \endcode
  */
  inline std::ostream & operator<<( std::ostream & str, const std::basic_ios<char> & obj )
  {
    std::string ret( "[" );
    ret += ( obj.good() ? 'g' : '_' );
    ret += ( obj.eof()  ? 'e' : '_' );
    ret += ( obj.fail() ? 'F' : '_' );
    ret += ( obj.bad()  ? 'B' : '_' );
    ret += "]";
    return str << ret;
  }
} // namespace std
///////////////////////////////////////////////////////////////////
namespace zypp {

  ///////////////////////////////////////////////////////////////////
  // iomanipulator: str << dump(val) << ...
  // calls:         std::ostream & dumpOn( std::ostream & str, const Type & obj )
  ///////////////////////////////////////////////////////////////////

  namespace detail
  {
    template<class Tp>
    struct Dump
    {
      Dump( const Tp & obj_r ) : _obj( obj_r ) {}
      const Tp & _obj;
    };

    template<class Tp>
    std::ostream & operator<<( std::ostream & str, const Dump<Tp> & obj )
    { return dumpOn( str, obj._obj ); }
  }

  template<class Tp>
  detail::Dump<Tp> dump( const Tp & obj_r )
  { return detail::Dump<Tp>(obj_r); }

  /** hexdump data on stream
   * \code
   * hexdump 0000000333 bytes (0x0000014d):
   * 0000: 0c 00 01 49 03 00 17 41 04 af 7c 75 5e 4c 2d f7 ...I...A..|u^L-.
   * 0010: c9 c9 75 bf a8 41 37 2a d0 03 2c ff 96 d2 43 89 ..u..A7*..,...C.
   * 0020: ...
   * \endcode
   */
  inline std::ostream & hexdumpOn( std::ostream & outs, const unsigned char *ptr, size_t size )
  {
    size_t i = 0,c = 0;
    unsigned width = 0x10;
    outs << str::form( "hexdump %10.10ld bytes (0x%8.8lx):\n", (long)size, (long)size );

    for ( i = 0; i < size; i += width ) {
      outs << str::form( "%4.4lx: ", (long)i );
      /* show hex to the left */
      for ( c = 0; c < width; ++c ) {
        if ( i+c < size )
          outs << str::form( "%02x ", ptr[i+c] );
        else
          outs << ("   ");
      }
      /* show data on the right */
      for ( c = 0; (c < width) && (i+c < size); ++c ) {
        char x = (ptr[i+c] >= 0x20 && ptr[i+c] < 0x7f) ? ptr[i+c] : '.';
        outs << x;
      }
      outs << std::endl;
    }
    return outs;
  }
  /** \overload */
  inline std::ostream & hexdumpOn( std::ostream & outs, const char *ptr, size_t size )
  { return hexdumpOn( outs, reinterpret_cast<const unsigned char*>(ptr), size ); }

} // namespace zypp
///////////////////////////////////////////////////////////////////
namespace std {
  /*!
   * Write type info to stream
   * @TODO de-inline me
   */
  inline std::ostream & operator<<( std::ostream & str, const std::type_info &info )
  {
#ifdef __GNUG__
    int status = -4; // some arbitrary value to eliminate the compiler warning

    // enable c++11 by passing the flag -std=c++11 to g++
    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(info.name(), NULL, NULL, &status),
        std::free
    };
    return str << std::string((status==0) ? res.get() : info.name());
#else
    return str << info.name();
#endif
  }

#ifdef __cpp_lib_optional // YAST/PK explicitly use c++11 until 15-SP3
  template<class Tp>
  inline std::ostream & operator<<( std::ostream & str, const std::optional<Tp> & obj )
  {
    if ( obj )
      str << "opt(" << *obj << ")";
    else
      str << "nullopt";
    return str;
  }
#endif
} // namespace std
///////////////////////////////////////////////////////////////////
#endif // ZYPP_BASE_LOGTOOLS_H
