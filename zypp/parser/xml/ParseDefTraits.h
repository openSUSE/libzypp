/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/parser/xml/ParseDefTraits.h
 *
*/
#ifndef ZYPP_PARSER_XML_PARSEDEFTRAITS_H
#define ZYPP_PARSER_XML_PARSEDEFTRAITS_H

#include <zypp/Bit.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace xml
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : ParseDefTraits
    //
    /** */
    struct ParseDefTraits
    {
      using ModeBitsType = unsigned char;
      using ModeBits = bit::BitField<ModeBitsType>;
      using TypeBits = bit::Range<ModeBitsType, 0, 1>;
      using VisitBits = bit::Range<ModeBitsType, TypeBits::end, 1>;

      enum TypeValue
        {
          BIT_OPTIONAL  = bit::RangeValue<TypeBits,0>::value,
          BIT_MANDTAORY = bit::RangeValue<TypeBits,1>::value
        };

      enum VisitValue
        {
          BIT_ONCE      = bit::RangeValue<VisitBits,0>::value,
          BIT_MULTIPLE  = bit::RangeValue<VisitBits,1>::value
        };
    };
    ///////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////
  } // namespace xml
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_PARSER_XML_PARSEDEFTRAITS_H
