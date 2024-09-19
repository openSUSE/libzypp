/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "GpgCheck.h"

namespace zypp {
  std::ostream & operator<<( std::ostream & str, const repo::GpgCheck & obj )
  {
    switch ( obj )
    {
#define OUTS( V ) case repo::V: return str << #V; break
      OUTS( GpgCheck::On );
      OUTS( GpgCheck::Strict );
      OUTS( GpgCheck::AllowUnsigned );
      OUTS( GpgCheck::AllowUnsignedRepo );
      OUTS( GpgCheck::AllowUnsignedPackage );
      OUTS( GpgCheck::Default );
      OUTS( GpgCheck::Off );
      OUTS( GpgCheck::indeterminate );
#undef OUTS
    }
    return str << "GpgCheck::UNKNOWN";
  }

}
