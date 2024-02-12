/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/target/TargetException.h
 *
*/
#ifndef ZYPP_TARGET_TARGETEXCEPTION_H
#define ZYPP_TARGET_TARGETEXCEPTION_H

#include <iosfwd>

#include <string>

#include <zypp/base/Exception.h>
#include <zypp/Pathname.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  namespace target {
    ///////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : TargetException
    /** Just inherits Exception to separate target exceptions
     *
     **/
    class TargetException : public Exception
    {
    public:
      /** Ctor taking message.
       * Use \ref ZYPP_THROW to throw exceptions.
      */
      TargetException()
      : Exception( "Target Exception" )
      {}
      /** Ctor taking message.
       * Use \ref ZYPP_THROW to throw exceptions.
      */
      TargetException( const std::string & msg_r )
      : Exception( msg_r )
      {}
      /** Dtor. */
      ~TargetException() throw() override {};
    };

    class TargetAbortedException : public TargetException
    {
    public:
      TargetAbortedException( );

      /** Ctor taking message.
       * Use \ref ZYPP_THROW to throw exceptions.
      */
      TargetAbortedException( const std::string & msg_r )
      : TargetException( msg_r )
      {}
      /** Dtor. */
      ~TargetAbortedException() throw() override {};
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    private:
    };


  /////////////////////////////////////////////////////////////////
  } // namespace target
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_TARGET_TARGETEXCEPTION_H
