/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/misc/CheckAccessDeleted.h
 *
*/
#ifndef ZYPP_MISC_CHECKACCESSDELETED_H
#define ZYPP_MISC_CHECKACCESSDELETED_H

#include <iosfwd>
#include <vector>
#include <string>
 #include "zypp/base/Macros.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  /**
   * Check for running processes which access deleted executables or libraries.
   *
   * Executed after commit, this gives a hint which processes/services
   * need to be restarted.
   *
   * Per default upon construtcion or explicit call to \ref check,
   * information about running processes which access deleted files
   * or libraries is collected and provided as a \ref ProcInfo
   * container.
   */
  class ZYPP_API CheckAccessDeleted
  {

    public:
      /**
       * Data about one running process accessing deleted files.
       */
      struct ProcInfo
      {
        std::string pid;		//!< process ID
        std::string ppid;		//!< parent process ID
        std::string puid;		//!< process user ID
        std::string login;		//!< process login name
        std::string command;		//!< process command name
        std::vector<std::string> files;	//!< list of deleted executables or libraries accessed

        /** Guess if command was started by an \c /etc/init.d/ script.
         * The name of an \c /etc/init.d/ script that might be used to restart the
         * command.
         * \warning This is just a guess.
        */
        std::string service() const;
      };

      typedef size_t   					size_type;
      typedef ProcInfo					value_type;
      typedef std::vector<ProcInfo>::const_iterator	const_iterator;

    public:
      /** Default ctor performs check immediately.
       * Pass \c false and the initial check is omitted.
       * \throws Exception if \ref check throws.
       * \see \ref check.
       */
      CheckAccessDeleted( bool doCheck_r = true )
      { if ( doCheck_r ) check(); }

    public:
      /** Check for running processes which access deleted executables or libraries.
       *
       * Per default \ref check will try guess and collect executables and
       * libraries only by looking at the files path and name. (e.g named
       * \c lib* or located in \c *bin/).
       *
       * A verbose check will omit this test and collect all processes uning
       * any deleted file.
       *
       * \return the number of processes found.
       * \throws Exception On error collecting the data (e.g. no lsof installed)
       */
      size_type check( bool verbose_r = false );

      bool empty() const		{ return _data.empty(); }
      size_type size() const		{ return _data.size(); }
      const_iterator begin() const	{ return _data.begin(); }
      const_iterator end() const	{ return _data.end(); }

    public:
      /** Guess if \c command was started by an \c /etc/init.d/ script.
       * The name of an \c /etc/init.d/ script that might be used to restart the
       * command. \c command may be specifies by name, full path or pid.
       * \warning This is just a guess.
       */
      static std::string findService( const char * command_r );
      /** \overload Taking a string.*/
      static std::string findService( const std::string & command_r );
      /** \overload Taking a pathname. */
      static std::string findService( const Pathname & command_r );
      /** \overload taking the pid. */
      static std::string findService( pid_t pid_r );

    private:
      std::vector<ProcInfo> _data;
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates CheckAccessDeleted Stream output */
  std::ostream & operator<<( std::ostream & str, const CheckAccessDeleted & obj );

  /** \relates CheckAccessDeleted::ProcInfo Stream output */
  std::ostream & operator<<( std::ostream & str, const CheckAccessDeleted::ProcInfo & obj );

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_MISC_CHECKACCESSDELETED_H
