/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_PARSER_PLAINDIR_REPOPARSER_H
#define ZYPP_PARSER_PLAINDIR_REPOPARSER_H

#include "zypp/macros.h"
#include "zypp/RepoStatus.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace parser
  { /////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////
    namespace plaindir
    { /////////////////////////////////////////////////////////////////

      /**
       * \short Gives a cookie for a dir
       */
      ZYPP_EXPORT RepoStatus dirStatus( const Pathname &dir );

      /////////////////////////////////////////////////////////////////
    } // namespace plaindir
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////
  } // namespace parser
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_PARSER_PLAINDIR_REPOPARSER_H
