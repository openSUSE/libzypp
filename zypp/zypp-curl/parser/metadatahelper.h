/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_CURL_PARSER_METADATAHELPER_H
#define ZYPP_CURL_PARSER_METADATAHELPER_H

/*!
 * \file contains helpers for detecting and handling download metadata...
 *       e.g. zsync or metalink
 */


#include <zypp-core/Pathname.h>
#include <cstdio>
#include <vector>

namespace zypp::media {

  enum class MetaDataType {
    None  = 0,
    Zsync,
    MetaLink
  };

  constexpr auto minMetalinkProbeSize = 256; //< The nr of bytes we download to decide if we got a metadata file or not

  MetaDataType looks_like_meta_data( const std::vector<char> &data );
  MetaDataType looks_like_meta_file( const zypp::Pathname &file );
  MetaDataType looks_like_meta_file( FILE *file );
}


#endif
