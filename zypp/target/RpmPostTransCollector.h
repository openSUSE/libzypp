/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/target/RpmPostTransCollector.h
 */
#ifndef ZYPP_TARGET_RPMPOSTTRANSCOLLECTOR_H
#define ZYPP_TARGET_RPMPOSTTRANSCOLLECTOR_H

#include <iosfwd>

#include <zypp/base/PtrTypes.h>
#include <zypp/Pathname.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace target
  {
    ///////////////////////////////////////////////////////////////////
    /// \class RpmPostTransCollector
    /// \brief Extract and remember %posttrans scripts for later execution
    /// \todo Maybe embedd this into the TransactionSteps.
    ///////////////////////////////////////////////////////////////////
    class RpmPostTransCollector
    {
      friend std::ostream & operator<<( std::ostream & str, const RpmPostTransCollector & obj );
      friend std::ostream & dumpOn( std::ostream & str, const RpmPostTransCollector & obj );

      public:
        /** Default ctor */
        RpmPostTransCollector( const Pathname & root_r );

        /** Dtor */
        ~RpmPostTransCollector();

      public:
        /** Extract and remember a packages %posttrans script for later execution.
         * \return whether a script was collected.
         */
        bool collectScriptFromPackage( const Pathname & rpmPackage_r );

        /** Execute the remembered scripts.
         * \return false if execution was aborted by a user callback
         */
        bool executeScripts();

        /** Discard all remembered scrips. */
        void discardScripts();

      public:
        class Impl;              ///< Implementation class.
      private:
        RW_pointer<Impl> _pimpl; ///< Pointer to implementation.
    };

    /** \relates RpmPostTransCollector Stream output */
    std::ostream & operator<<( std::ostream & str, const RpmPostTransCollector & obj );

    /** \relates RpmPostTransCollector Verbose stream output */
    std::ostream & dumOn( std::ostream & str, const RpmPostTransCollector & obj );

  } // namespace target
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_TARGET_RPMPOSTTRANSCOLLECTOR_H
