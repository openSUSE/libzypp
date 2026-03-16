/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPP_NG_LOG_SAT_SOLVABLESPEC_H_INCLUDED
#define ZYPP_NG_LOG_SAT_SOLVABLESPEC_H_INCLUDED

#include <zypp/ng/log/format.h>
#include <iosfwd>

namespace zyppng {

  namespace sat {
    class SolvableSpec;
    class EvaluatedSolvableSpec;
  }

  namespace log {
    /**
     * \brief Formatter for \ref zyppng::sat::SolvableSpec.
     *
     * Produces a compact human-readable representation listing the ident
     * set and the provides set, e.g.:
     * \code
     *   SolvableSpec{idents:[kernel,glibc] provides:[provides:kernel-devel]}
     * \endcode
     */
    template <>
    struct formatter<sat::SolvableSpec> {
        static std::ostream& stream( std::ostream& str, const sat::SolvableSpec & obj );
    };

    /**
     * \brief Formatter for \ref zyppng::sat::EvaluatedSolvableSpec.
     *
     * Produces a compact summary of the resolved id count, e.g.:
     * \code
     *   EvaluatedSolvableSpec{ids:3}
     * \endcode
     * The raw ids are not printed as they are meaningless without a pool.
     */
    template <>
    struct formatter<sat::EvaluatedSolvableSpec> {
        static std::ostream& stream( std::ostream& str, const sat::EvaluatedSolvableSpec & obj );
    };
  }

}
#endif // ZYPP_NG_LOG_SAT_SOLVABLESPEC_H_INCLUDED
