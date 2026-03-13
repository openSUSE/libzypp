/*---------------------------------------------------------------------
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
#ifndef ZYPPNG_SAT_SOLVABLEIDENT_H_INCLUDED
#define ZYPPNG_SAT_SOLVABLEIDENT_H_INCLUDED

#include <zypp/ng/idstring.h>
#include <zypp/ng/reskind.h>

namespace zyppng::sat {

    /**
     * \class SplitIdent
     * \brief Helper that splits an identifier into kind and name or vice versa.
     *
     *
     * \note In case \c name_r is preceded by a well known kind spec, the
     * \c kind_r argument is ignored, and kind is derived from name.
     */
    class SplitIdent
    {
      public:
        SplitIdent() = default;
        SplitIdent( IdString ident_r );
        SplitIdent( const char * ident_r );
        SplitIdent( const std::string & ident_r );
        SplitIdent( ResKind kind_r, IdString name_r );
        SplitIdent( ResKind kind_r, const zypp::C_Str & name_r );

        IdString ident() const { return _ident; }
        ResKind  kind()  const { return _kind; }
        IdString name()  const { return _name; }

      private:
        IdString  _ident;
        ResKind   _kind;
        IdString  _name;
    };

} // namespace zyppng::sat

#endif
