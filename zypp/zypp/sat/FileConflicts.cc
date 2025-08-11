/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/FileConflicts.cc
 */
#include <iostream>
#include <string>

#include <zypp/sat/FileConflicts.h>
#include <zypp/base/LogTools.h>
#include <zypp/base/Gettext.h>
#include <zypp/base/Xml.h>
#include <zypp/CheckSum.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace sat
  {
    std::string FileConflicts::Conflict::asUserString() const
    {
      if ( lhsFilename() == rhsFilename() )
      {
        static const char * text[2][2] = {{ // [lhs][rhs] 0 = installed; 1 = to be installed
            // TranslatorExplanation %1%(filename) %2%(package1) %3%(package2)
            N_( "File %1%\n"
                "  from package\n"
                "     %2%\n"
                "  conflicts with file from package\n"
                "     %3%" ),
            // TranslatorExplanation %1%(filename) %2%(package1) %3%(package2)
            N_( "File %1%\n"
                "  from package\n"
                "     %2%\n"
                "  conflicts with file from install of\n"
                "     %3%" )
        },{
            // TranslatorExplanation %1%(filename) %2%(package1) %3%(package2)
            N_( "File %1%\n"
                "  from install of\n"
                "     %2%\n"
                "  conflicts with file from package\n"
                "     %3%" ),
            // TranslatorExplanation %1%(filename) %2%(package1) %3%(package2)
            N_( "File %1%\n"
                "  from install of\n"
                "     %2%\n"
                "  conflicts with file from install of\n"
                "     %3%" )
        }};
        return str::Format( text[lhsSolvable().isSystem()?0:1][rhsSolvable().isSystem()?0:1] )
               % lhsFilename()
               % lhsSolvable().asUserString()
               % rhsSolvable().asUserString();
      }
      else
      {
        static const char * text[2][2] = {{ // [lhs][rhs] 0 = installed; 1 = to be installed
            // TranslatorExplanation %1%(filename1) %2%(package1) %%3%(filename2) 4%(package2)
            N_( "File %1%\n"
                "  from package\n"
                "     %2%\n"
                "  conflicts with file\n"
                "     %3%\n"
                "  from package\n"
                "     %4%" ),
            // TranslatorExplanation %1%(filename1) %2%(package1) %3%(filename2) %4%(package2)
            N_( "File %1%\n"
                "  from package\n"
                "     %2%\n"
                "  conflicts with file\n"
                "     %3%\n"
                "  from install of\n"
                "     %4%" )
        },{
            // TranslatorExplanation %1%(filename1) %2%(package1) %3%(filename2) %4%(package2)
            N_( "File %1%\n"
                "  from install of\n"
                "     %2%\n"
                "  conflicts with file\n"
                "     %3%\n"
                "  from package\n"
                "     %4%" ),
            // TranslatorExplanation %1%(filename1) %2%(package1) %3%(filename2) %4%(package2)
            N_( "File %1%\n"
                "  from install of\n"
                "     %2%\n"
                "  conflicts with file\n"
                "     %3%\n"
                "  from install of\n"
                "     %4%" )
        }};
        return str::Format( text[lhsSolvable().isSystem()?0:1][rhsSolvable().isSystem()?0:1] )
               % lhsFilename()
               % lhsSolvable().asUserString()
               % rhsFilename()
               % rhsSolvable().asUserString();
      }
    }

    std::ostream & operator<<( std::ostream & str, const FileConflicts & obj )
    { return dumpRange( str << "(" << obj.size() << ") ", obj.begin(), obj.end() ); }

    std::ostream & operator<<( std::ostream & str, const FileConflicts::Conflict & obj )
    {
      if ( obj.lhsFilename() == obj.rhsFilename() )
        return str << str::Format( "%s:\n    %s[%s]\n    %s[%s]" )
                      % obj.lhsFilename()
                      % obj.lhsSolvable()
                      % obj.lhsFilemd5()
                      % obj.rhsSolvable()
                      % obj.rhsFilemd5();

      return str << str::Format( "%s - %s:\n    %s[%s]\n    %s[%s]" )
                    % obj.lhsFilename()
                    % obj.rhsFilename()
                    % obj.lhsSolvable()
                    % obj.lhsFilemd5()
                    % obj.rhsSolvable()
                    % obj.rhsFilemd5();
    }


    std::ostream & dumpAsXmlOn( std::ostream & str, const FileConflicts & obj )
    {
      xmlout::Node guard( str, "fileconflicts", { "size", obj.size() } );
      if ( ! obj.empty() )
      {
        *guard << "\n";
        for ( const auto & el : obj )
          dumpAsXmlOn( *guard, el ) << "\n";
      }
      return str;
    }

    namespace
    {
      std::ostream & dumpAsXmlHelper( std::ostream & str, const std::string & tag_r, IdString filename_r, IdString md5sum_r, Solvable solv_r )
      {
        xmlout::Node guard( str, tag_r );
        *xmlout::Node( *guard, "file" ) << filename_r;
        dumpAsXmlOn( *guard, CheckSum( md5sum_r.asString() ) );
        dumpAsXmlOn( *guard, solv_r );
        return str;
      }
    }

    std::ostream & dumpAsXmlOn( std::ostream & str, const FileConflicts::Conflict & obj )
    {
      xmlout::Node guard( str, "fileconflict" );
      dumpAsXmlHelper( *guard<<"\n", "lhs", obj.lhsFilename(), obj.lhsFilemd5(), obj.lhsSolvable() );
      dumpAsXmlHelper( *guard<<"\n", "rhs", obj.rhsFilename(), obj.rhsFilemd5(), obj.rhsSolvable() );
      return str;
    }

  } // namespace sat
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
