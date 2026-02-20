/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp-core/parser/econfdict.h
 */
#ifndef ZYPP_PARSER_ECONFDICT_H
#define ZYPP_PARSER_ECONFDICT_H

#include <zypp-core/parser/IniDict>
#include <zypp-core/parser/ParseException>
#include <zypp-core/Pathname.h>

namespace zypp::parser {

  /**
   * Parse the prioritized list of files and drop-ins to read
   * and merge for a specific config file stem:
   *   [PROJECT/]EXAMPLE[.SUFFIX]
   *   e.g: zypp/zypp.conf
   *
   * The rules are defined by the UAPI.6 Configuration Files
   * Specification (version 1)[1], but may be changed to follow
   * newer versions in the future.
   *
   * \throws \ref EconfException if \a stem_r is malformed
   * \throws \ref ParseException thrown from the underlying \ref IniDict
   *
   * \see [1] https://github.com/uapi-group/specifications/blob/main/specs/configuration_files_specification.md
   */
  struct EconfDict : public IniDict
  {
    EconfDict();
    EconfDict( const std::string & stem_r, const Pathname & root_r = Pathname("/") );

    static Pathname defaultDistconfDir();               ///< Where the vendor configuration files live (APIConfig(LIBZYPP_ZYPPCONFDIR))
    static void defaultDistconfDir( Pathname path_r );  ///< Testing: Set an alternate default path for the vendor configuration files
  };
} // namespace zypp::parser
#endif // ZYPP_PARSER_ECONFDICT_H
