/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ShutdownLock_p.h
 *
*/

#ifndef ZYPP_SHUTDOWNLOCK_P_H_INCLUDED
#define ZYPP_SHUTDOWNLOCK_P_H_INCLUDED

#include <string>

#include <zypp-core/Globals.h>
#include <zypp-core/base/PtrTypes.h>

namespace zypp
{

class ExternalProgramWithSeperatePgid;

/**
 * Attempts to create a lock to prevent the system
 * from going into hibernate/shutdown. The lock is automatically
 * released when the object is destroyed.
 *
 * \note Preferably derive ShutdownLocks for a specific purpose
 * and use a unique reason string. External tools may want to test
 * for the presence of this lock.
 */
class ShutdownLock
{
protected:
  ShutdownLock( const std::string &who, const std::string &reason );
   ~ShutdownLock();

private:
   shared_ptr<ExternalProgramWithSeperatePgid> _prog;
};

class ShutdownLockCommit : private ShutdownLock
{
public:
  ShutdownLockCommit( const std::string &who )
  : ShutdownLock( who, "Zypp commit running." )
  {}
};

} // namespace
#endif
