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
module;
#include <iosfwd>

export module zyppng:log_sat_solvablespec;

import :log_format;
import :sat_solvablespec;  // brings SolvableSpec, EvaluatedSolvableSpec — do NOT forward-declare them here

export namespace zyppng {

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
