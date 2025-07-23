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
#include <zypp/IdString.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace target
  {
    namespace rpm { class RpmDb; }

    ///////////////////////////////////////////////////////////////////
    /// \class RpmPostTransCollector
    /// \brief Extract and remember %posttrans scripts for later execution
    ///
    /// bsc#1041742: Attempt to delay also %transfiletrigger(postun|in) execution
    /// iff rpm supports it. Rpm versions supporting --runposttrans will inject
    /// "dump_posttrans:..." lines into the output if macro "_dump_posttrans" is
    /// defined during execution. Those lines are collected and later fed into
    /// "rpm --runposttrans".
    /// If rpm does not support it, those lines are not injected. In this case we
    /// collect and later execute the %posttrans script on our own.
    ///////////////////////////////////////////////////////////////////
    class RpmPostTransCollector
    {
      friend std::ostream & operator<<( std::ostream & str, const RpmPostTransCollector & obj );
      friend std::ostream & dumpOn( std::ostream & str, const RpmPostTransCollector & obj );

      public:
        /** Default ctor */
        RpmPostTransCollector( Pathname root_r );

        /** Dtor */
        ~RpmPostTransCollector();

      public:
        /** Test whether a package defines a %posttrans script. */
        bool hasPosttransScript( const Pathname & rpmPackage_r );

        /** Extract and remember a packages %posttrans script or dump_posttrans lines for later execution. */
        void collectPosttransInfo( const Pathname & rpmPackage_r, const std::vector<std::string> & runposttrans_r );
        /** \overload 'remove' does not trigger a %posttrans, but it may trigger %transfiletriggers. */
        void collectPosttransInfo( const std::vector<std::string> & runposttrans_r );

        /** Execute the remembered scripts and/or or dump_posttrans lines. */
        void executeScripts( rpm::RpmDb & rpm_r, const IdStringSet & obsoletedPackages_r );

        /** Discard all remembered scripts and/or or dump_posttrans lines. */
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
