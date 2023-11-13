/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "metadatahelper.h"

#include <zypp-core/base/IOTools.h>
#include <memory>

namespace zypp::media {

  MetaDataType looks_like_meta_data(const std::vector<char> &data)
  {
    if ( data.empty() )
      return MetaDataType::None;

    const char *p = data.data();
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
      p++;

    // If we have a zsync file, it has to start with zsync:
    if ( !strncasecmp( p, "zsync:", 6 ) ) {
      return MetaDataType::Zsync;
    }

    if (!strncasecmp(p, "<?xml", 5))
    {
      while (*p && *p != '>')
        p++;
      if (*p == '>')
        p++;
      while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    }
    bool ret = !strncasecmp( p, "<metalink", 9 ) ? true : false;
    if ( ret )
      return MetaDataType::MetaLink;

    return MetaDataType::None;
  }

  MetaDataType looks_like_meta_file(const Pathname &file)
  {
    std::unique_ptr<FILE, decltype(&fclose)> fd( fopen( file.c_str(), "r" ), &fclose );
    return looks_like_meta_file( fd.get() );
  }

  MetaDataType looks_like_meta_file(FILE *file)
  {
    if ( !file )
      return MetaDataType::None;
    return looks_like_meta_data( io::peek_data_fd( file, 0, minMetalinkProbeSize )  );
  }

}
