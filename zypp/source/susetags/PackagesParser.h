/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/source/susetags/PackagesParser.h
 *
*/
#ifndef ZYPP_SOURCE_SUSETAGS_PACKAGESPARSER_H
#define ZYPP_SOURCE_SUSETAGS_PACKAGESPARSER_H

#include <iosfwd>
#include <list>

#include "zypp/Pathname.h"
#include "zypp/Package.h"
#include "zypp/source/susetags/SuseTagsImpl.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace source
  { /////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////
    namespace susetags
    { /////////////////////////////////////////////////////////////////

      /** \deprecated Just temporary.
       * \throws ParseException and others.
      */
      std::list<Package::Ptr> parsePackages( Source_Ref source_r, SuseTagsImpl::Ptr, Pathname & file_r );

      /////////////////////////////////////////////////////////////////
    } // namespace susetags
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////
  } // namespace source
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SOURCE_SUSETAGS_PACKAGESPARSER_H
