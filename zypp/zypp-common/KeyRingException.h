/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp-common/KeyRingException.h
 *
*/
#ifndef ZYPP_KEYRING_EXCEPTION_H
#define ZYPP_KEYRING_EXCEPTION_H

#include <zypp-core/Globals.h>
#include <zypp-core/base/Exception.h>

namespace zypp {

  class ZYPP_API KeyRingException : public Exception
   {
     public:
       /** Ctor taking message.
      * Use \ref ZYPP_THROW to throw exceptions.
        */
       KeyRingException()
       : Exception( "Bad Key Exception" )
       {}
       /** Ctor taking message.
        * Use \ref ZYPP_THROW to throw exceptions.
        */
       KeyRingException( const std::string & msg_r )
       : Exception( msg_r )
       {}
       /** Dtor. */
       ~KeyRingException() throw() override {};
   };

}

#endif
