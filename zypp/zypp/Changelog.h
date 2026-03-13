/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/Changelog.h
 *
*/
#ifndef ZYPP_CHANGELOG_H
#define ZYPP_CHANGELOG_H

#include <string>
#include <list>
#include <utility>

#include <zypp-core/Globals.h>
#include <zypp-core/Date.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ChangelogEntry
  //
  /** Single entry in a change log
  */
  class ChangelogEntry
  {
  public:
    /** Default ctor */
    ChangelogEntry( const Date & d,
                    std::string  a,
                    std::string  t )
    : _date( d ), _author(std::move( a )), _text(std::move( t ))
    {};
    /** Dtor */
    ~ChangelogEntry()
    {}
    Date date() const { return _date; }
    std::string author() const { return _author; }
    std::string text() const { return _text; }

  private:
    Date _date;
    std::string _author;
    std::string _text;
  };

  /** List of ChangelogEntry. */
  using Changelog = std::list<ChangelogEntry>;

  /** \relates ChangelogEntry */
  std::ostream & operator<<( std::ostream & out, const ChangelogEntry & obj ) ZYPP_API;

  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////

#endif // ZYPP_CHANGELOG_H
