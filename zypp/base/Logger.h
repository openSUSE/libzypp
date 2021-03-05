/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/base/Logger.h
 *
*/
#ifndef ZYPP_BASE_LOGGER_H
#define ZYPP_BASE_LOGGER_H
#include <cstring>
#include <iosfwd>
#include <string>

///////////////////////////////////////////////////////////////////
#ifndef ZYPP_NDEBUG
namespace zypp
{
  namespace debug
  { // impl in LogControl.cc

    // Log code loacaton and block leave
    // Indent if nested
    struct TraceLeave
    {
      TraceLeave( const TraceLeave & ) =delete;
      TraceLeave & operator=( const TraceLeave & ) =delete;
      TraceLeave( const char * file_r, const char * fnc_r, int line_r );
      ~TraceLeave();
    private:
      static unsigned _depth;
      const char *    _file;
      const char *    _fnc;
      int             _line;
    };
#define TRACE ::zypp::debug::TraceLeave _TraceLeave( __FILE__, __FUNCTION__, __LINE__ )

    // OnScreenDebug messages colored to stderr
    struct Osd
    {
      Osd( int = 0 );
      ~Osd();

      template<class Tp>
      Osd & operator<<( Tp && val )
      { _str << std::forward<Tp>(val); return *this; }

      Osd & operator<<( std::ostream& (*iomanip)( std::ostream& ) );

    private:
      std::ostream & _str;
    };
#define OSD ::zypp::debug::Osd()
  }
}
#endif // ZYPP_NDEBUG
///////////////////////////////////////////////////////////////////

/** \defgroup ZYPP_BASE_LOGGER_MACROS ZYPP_BASE_LOGGER_MACROS
 *  Convenience macros for logging.
 *
 * The macros finaly call @ref getStream, providing appropriate arguments,
 * to return the log stream.
 *
 * @code
 * L_DBG("foo") << ....
 * @endcode
 * Logs a debug message for group @a "foo".
 *
 * @code
 * #undef ZYPP_BASE_LOGGER_LOGGROUP
 * #define ZYPP_BASE_LOGGER_LOGGROUP "foo"
 *
 * DBG << ....
 * @endcode
 * Defines group @a "foo" as default for log messages and logs a
 * debug message.
 */
/*@{*/

#ifndef ZYPP_BASE_LOGGER_LOGGROUP
/** Default log group if undefined. */
#define ZYPP_BASE_LOGGER_LOGGROUP "DEFINE_LOGGROUP"
#endif

#define XXX L_XXX( ZYPP_BASE_LOGGER_LOGGROUP )
#define DBG L_DBG( ZYPP_BASE_LOGGER_LOGGROUP )
#define MIL L_MIL( ZYPP_BASE_LOGGER_LOGGROUP )
#define WAR L_WAR( ZYPP_BASE_LOGGER_LOGGROUP )
#define ERR L_ERR( ZYPP_BASE_LOGGER_LOGGROUP )
#define SEC L_SEC( ZYPP_BASE_LOGGER_LOGGROUP )
#define INT L_INT( ZYPP_BASE_LOGGER_LOGGROUP )
#define USR L_USR( ZYPP_BASE_LOGGER_LOGGROUP )

#define L_XXX(GROUP) ZYPP_BASE_LOGGER_LOG( GROUP, zypp::base::logger::E_XXX )
#define L_DBG(GROUP) ZYPP_BASE_LOGGER_LOG( GROUP"++", zypp::base::logger::E_MIL )
#define L_MIL(GROUP) ZYPP_BASE_LOGGER_LOG( GROUP, zypp::base::logger::E_MIL )
#define L_WAR(GROUP) ZYPP_BASE_LOGGER_LOG( GROUP, zypp::base::logger::E_WAR )
#define L_ERR(GROUP) ZYPP_BASE_LOGGER_LOG( GROUP, zypp::base::logger::E_ERR )
#define L_SEC(GROUP) ZYPP_BASE_LOGGER_LOG( GROUP, zypp::base::logger::E_SEC )
#define L_INT(GROUP) ZYPP_BASE_LOGGER_LOG( GROUP, zypp::base::logger::E_INT )
#define L_USR(GROUP) ZYPP_BASE_LOGGER_LOG( GROUP, zypp::base::logger::E_USR )

#define L_BASEFILE ( *__FILE__ == '/' ? strrchr( __FILE__, '/' ) + 1 : __FILE__ )

/** Actual call to @ref getStream. */
#define ZYPP_BASE_LOGGER_LOG(GROUP,LEVEL) \
        zypp::base::logger::getStream( GROUP, LEVEL, L_BASEFILE, __FUNCTION__, __LINE__ )

/*@}*/

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace base
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    namespace logger
    { /////////////////////////////////////////////////////////////////

      /** Definition of log levels.
       *
       * @see getStream
      */
      enum LogLevel {
        E_XXX = 999, /**< Excessive logging. */
        E_DBG = 0,   /**< Debug or verbose. */
        E_MIL,       /**< Milestone. */
        E_WAR,       /**< Warning. */
        E_ERR,       /**< Error. */
        E_SEC,       /**< Secutrity related. */
        E_INT,       /**< Internal error. */
        E_USR        /**< User log. */
      };

      /** Return a log stream to write on.
       *
       * The returned log stream is determined by @a group_r and
       * @a level_r. The remaining arguments @a file_r, @a func_r
       * and @a line_r are expected to denote the location in the
       * source code that issued the message.
       *
       * @note You won't call @ref getStream directly, but use the
       * @ref ZYPP_BASE_LOGGER_MACROS.
      */
      extern std::ostream & getStream( const char * group_r,
                                       LogLevel     level_r,
                                       const char * file_r,
                                       const char * func_r,
                                       const int    line_r );
      extern bool isExcessive();

      /////////////////////////////////////////////////////////////////
    } // namespace logger
    ///////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////
  } // namespace base
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_BASE_LOGGER_H
