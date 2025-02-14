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
#ifndef ZYPPNG_CURL_ZCKHELPER_H_INCLUDED
#define ZYPPNG_CURL_ZCKHELPER_H_INCLUDED

#include <zypp-curl/ng/network/curlmultiparthandler.h>
#include <zypp-curl/ng/network/rangedesc.h>

namespace zyppng {

  class ZckHelper {
  public:

    using Block = RangeDesc;

    struct PrepareResult {
      enum Code {
        Error,          // we got an error
        NothingToDo,    // Target file is already complete
        ExceedsMaxLen,  // Zchunk header reports a different filesize than what was expected
        Success         // Returns a list of blocks to fetch
      };

      Code _code;
      std::vector<Block> _blocks;
      zypp::ByteCount _bytesReused;
      std::string _message;
    };

    /*!
     * Checks if a given file is a zck file
     */
    static bool isZchunkFile(const zypp::Pathname &file);

    /*!
     * Prepares the file in \a target with already existing chunks in \a delta. The \a target file must
     * already contain the zck header data of the to be downloaded file.
     */
    static PrepareResult prepareZck ( const zypp::Pathname &delta, const zypp::Pathname &target, const zypp::ByteCount &expectedFileSize );

    /*!
     * Checks if a given zck file is internally valid
     */
    static bool validateZckFile( const zypp::Pathname &file, std::string &error );
  };
}
#endif
