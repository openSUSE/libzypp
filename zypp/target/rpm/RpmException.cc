/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/target/rpm/RpmException.cc
 *
*/

#include <string>
#include <iostream>

#include <zypp/target/rpm/RpmException.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
namespace target
{
/////////////////////////////////////////////////////////////////
namespace rpm
{
/////////////////////////////////////////////////////////////////

std::ostream & RpmInvalidRootException::dumpOn( std::ostream & str ) const
{
  return str << "Illegal root " << _root
         << " or dbPath " << _dbpath;
}

std::ostream & RpmAccessBlockedException::dumpOn( std::ostream & str ) const
{
  return str << "Access is blocked: Root: " << _root
         << " dbPath: " << _dbpath;
}

std::ostream & RpmSubprocessException::dumpOn( std::ostream & str ) const
{
  return str << "Subprocess failed. Error: " << _errmsg;
}

std::ostream & RpmInitException::dumpOn( std::ostream & str) const
{
  return str << "Failed to initialize database: Root: " << _root
         << " dbPath: " << _dbpath;
}

std::ostream & RpmDbOpenException::dumpOn( std::ostream & str) const
{
  return str << "Failed to open database: Root: " << _root
         << " dbPath: " << _dbpath;
}

std::ostream & RpmDbAlreadyOpenException::dumpOn( std::ostream & str) const
{
  return str << "Can't switch to " << _new_root << " " << _new_dbpath
         << " while accessing " << _old_root << " " << _old_dbpath;
}

std::ostream & RpmDbNotOpenException::dumpOn( std::ostream & str) const
{
  return str << "RPM database not open";
}

std::ostream & RpmDbConvertException::dumpOn( std::ostream & str) const
{
  return str << "RPM database conversion failed";
}

std::ostream & RpmNullDatabaseException::dumpOn( std::ostream & str) const
{
  return str << "NULL rpmV4 database passed as argument!";
}

/////////////////////////////////////////////////////////////////
} // namespace rpm
} // namespace target
} // namespace zypp
///////////////////////////////////////////////////////////////////
