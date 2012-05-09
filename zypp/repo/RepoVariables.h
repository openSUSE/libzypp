/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_REPO_VARIABLES_H_
#define ZYPP_REPO_VARIABLES_H_

#include <string>
#include "zypp/base/Macros.h"
#include "zypp/Url.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace repo
  {

    /**
     * \short Functor replacing repository variables
     *
     * Replaces '$arch', '$basearch' and $releasever in a string
     * with the global ZYpp values.
     * \code
     * Example:
     * ftp://user:secret@site.net/$arch/ -> ftp://user:secret@site.net/i686/
     * http://site.net/?basearch=$basearch -> http://site.net/?basearch=i386
     * \endcode
     */
    struct ZYPP_API RepoVariablesStringReplacer : public std::unary_function<const std::string &, std::string>
    {
      std::string operator()( const std::string & value_r ) const;
    };

    /**
     * \short Functor replacing repository variables
     *
     * Replaces repository variables in the path and query part of the URL.
     * \see RepoVariablesStringReplacer
     */
    struct ZYPP_API RepoVariablesUrlReplacer : public std::unary_function<const Url &, Url>
    {
      Url operator()( const Url & url_r ) const;
    };

  } // namespace repo
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////

#endif
