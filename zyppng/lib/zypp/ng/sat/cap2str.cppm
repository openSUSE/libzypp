/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
/**
 * This file contains private API, this might break at any time between releases.
 * You have been warned!
 *
 */
module;
#include <string>

export module zyppng:sat_cap2str;

import :sat_poolconstants;

// Module-purview-only — internal helpers used by other zyppng partitions
// (capability.cc, log/sat/capability.cc).  Not exported to consumers.
namespace zyppng::sat::detail {

  /**
   * \brief Shared logic to convert a sat ID into a human readable string.
   * \details Handles recursive rich dependencies and architecture/namespace tagging.
   *
   * \return Returns a char array in a private memory area, this can be overridden when cap2str is called again
   */
  const char * cap2str( CPool * pool_r, IdType id_r, int parop_r = 0 );

  /**
   * \brief Shared logic to convert a sat ID into a human readable string.
   * \details Handles recursive rich dependencies and architecture/namespace tagging.
   * \param outs_r  The string to append the result to.
   * \param pool_r  The pool to use for ID lookups.
   * \param id_r    The ID to convert.
   * \param parop_r Parent operator for recursive calls (determines bracing).
   */
  void cap2str( std::string & outs_r, CPool * pool_r, IdType id_r, int parop_r );

  /**
   * \brief Convert a relation operator to its string representation.
   * \param op_r The operator ID (e.g., REL_GT, REL_AND).
   * \return String representation like " > " or " and ".
   */
  const char *capRel2Str( int op_r );

} // namespace zyppng::sat::detail
