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
#ifndef ZYPP_CURL_NG_NETWORK_RANGEDESC_INCLUDED
#define ZYPP_CURL_NG_NETWORK_RANGEDESC_INCLUDED

#include <zypp-core/zyppng/core/bytearray.h>
#include <optional>
#include <string>
#include <sys/types.h>

namespace zyppng {

  /*!
   * Describes a range of a file that is to be downloaded.
   */
  struct RangeDesc {
    size_t _start = 0;
    size_t _len = 0;

    /**
    * Enables automated checking of downloaded contents against a checksum.
    * \note expects _checksum in byte NOT in string format
    */
    std::string _chksumtype;
    UByteArray  _checksum;
    std::optional<size_t> _relevantDigestLen; //< initialized if only the first few bytes of the checksum should be considered
    std::optional<size_t> _chksumPad; //< initialized if the hashed blocks for a digest need to be padded if a block is smaller ( e.g. last block in a zsync file )
  };
}

#endif
