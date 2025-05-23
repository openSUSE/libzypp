/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/Env.h
 */
#ifndef ZYPP_BASE_ENV_H
#define ZYPP_BASE_ENV_H

#include <cstdlib>
#include <string>
#include <memory>
#include <zypp-core/TriBool.h>
#include <zypp-core/base/String.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace env
  {
    /**
     * If the environment variable \a var_r is set to a legal \c true or \c false string return \c bool, else \c indeterminate.
     *
     * True is:  "1", "yes", "true",  "always", "on", "+", a number !=0
     * False is: "0", "no",  "false", "never",  "off" "-"
     */
    inline TriBool getenvBool( const C_Str & var_r )
    {
      const char * v = ::getenv( var_r.c_str() );
      if ( v )
        return str::strToTriBool( v );
      return indeterminate;
    }

    ///////////////////////////////////////////////////////////////////
    /// \class ScopedSet
    /// \brief Temporarily set/unset an environment variable
    /// \ingroup g_RAII
    struct ScopedSet
    {
      ScopedSet( const ScopedSet & )             = delete;
      ScopedSet & operator=( const ScopedSet & ) = delete;

      ScopedSet( ScopedSet && )                  = default;
      ScopedSet & operator=( ScopedSet && )      = default;

    public:
      /** Default ctor (NOOP). */
      ScopedSet()
      {}

      /** Set \a var_r to \a val_r (unsets \a var_r if \a val_r is a \c nullptr). */
      ScopedSet( std::string var_r, const char * val_r )
      : _var { std::move(var_r) }
      {
        if ( !_var.empty() )
        {
          if ( const char * orig = ::getenv( _var.c_str() ) )
            _val.reset( new std::string( orig ) );
          setval( val_r );
        }
      }

      /** Restore the original setting. */
      ~ScopedSet()
      {
        if ( !_var.empty() )
          setval( _val ? _val->c_str() : nullptr );
      }

    private:
      void setval( const char * val_r )
      {
        if ( val_r )
          ::setenv( _var.c_str(), val_r, 1 );
        else
          ::unsetenv( _var.c_str() );
      }

    private:
      std::string _var;
      std::unique_ptr<std::string> _val;
    };

  } // namespace env
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_BASE_ENV_H
