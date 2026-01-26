/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/target/TargetCallbackReceiver.h
 *
*/
#ifndef ZYPP_TARGET_TARGETCALLBACKRECEIVER_H
#define ZYPP_TARGET_TARGETCALLBACKRECEIVER_H

#include <zypp/ZYppCallbacks.h>
#include <zypp/target/rpm/RpmCallbacks.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace target
  { /////////////////////////////////////////////////////////////////

    class RpmInstallPackageReceiver
        : public callback::ReceiveReport<rpm::RpmInstallReport>
    {
        callback::SendReport <rpm::InstallResolvableReport> _report;
        Resolvable::constPtr _resolvable;
        target::rpm::InstallResolvableReport::RpmLevel _level;
        bool _abort;
        std::string _finishInfo;

      public:

        RpmInstallPackageReceiver (Resolvable::constPtr res);
        virtual ~RpmInstallPackageReceiver ();

        void reportbegin() override;

        void reportend() override;

        /** Forwards generic reports. */
        void report( const UserData & userData_r ) override;

        /** Start the operation */
        void start( const Pathname & name ) override;

        void tryLevel( target::rpm::InstallResolvableReport::RpmLevel level_r );

        bool aborted() const { return _abort; }

        /**
         * Inform about progress
         * Return true on abort
         */
        bool progress( unsigned percent ) override;

        /** inform user about a problem */
        rpm::RpmInstallReport::Action problem( Exception & excpt_r ) override;

        /** Additional rpm output to be reported in \ref finish in case of success. */
        void finishInfo( const std::string & info_r ) override;

        /** Finish operation in case of success */
        void finish() override;

        /** Finish operation in case of fail, report fail exception */
        void finish( Exception & excpt_r ) override;
    };

    class RpmRemovePackageReceiver
        : public callback::ReceiveReport<rpm::RpmRemoveReport>
    {
        callback::SendReport <rpm::RemoveResolvableReport> _report;
        Resolvable::constPtr _resolvable;
        bool _abort;
        std::string _finishInfo;

      public:

        RpmRemovePackageReceiver (Resolvable::constPtr res);
        virtual ~RpmRemovePackageReceiver ();

        void reportbegin() override;

        void reportend() override;

        /** Start the operation */
        /** Forwards generic reports. */
        void report( const UserData & userData_r ) override;

        void start( const std::string & name ) override;

        /**
         * Inform about progress
         * Return true on abort
         */
        bool progress( unsigned percent ) override;

        /**
         *  Returns true if removing is aborted during progress
         */
        bool aborted() const { return _abort; }

        /** inform user about a problem */
        rpm::RpmRemoveReport::Action problem( Exception & excpt_r ) override;

        /** Additional rpm output to be reported in \ref finish in case of success. */
        void finishInfo( const std::string & info_r ) override;

        /** Finish operation in case of success */
        void finish() override;

        /** Finish operatin in case of fail, report fail exception */
        void finish( Exception & excpt_r ) override;
    };

    /////////////////////////////////////////////////////////////////
  } // namespace target
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_TARGET_TARGETCALLBACKRECEIVER_H
