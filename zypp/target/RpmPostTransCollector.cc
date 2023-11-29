/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/target/RpmPostTransCollector.cc
 */
#include <iostream>
#include <fstream>
#include <optional>
#include <zypp/base/LogTools.h>
#include <zypp/base/NonCopyable.h>
#include <zypp/base/Gettext.h>
#include <zypp/base/Regex.h>
#include <zypp/base/IOStream.h>
#include <zypp/base/InputStream.h>
#include <zypp/target/RpmPostTransCollector.h>

#include <zypp/TmpPath.h>
#include <zypp/PathInfo.h>
#include <zypp/HistoryLog.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/ExternalProgram.h>
#include <zypp/target/rpm/RpmDb.h>
#include <zypp/target/rpm/librpmDb.h>
#include <zypp/ZConfig.h>
#include <zypp/ZYppCallbacks.h>

using std::endl;
#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::posttrans"

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace target
  {
    ///////////////////////////////////////////////////////////////////
    /// \class RpmPostTransCollector::Impl
    /// \brief RpmPostTransCollector implementation.
    ///////////////////////////////////////////////////////////////////
    class RpmPostTransCollector::Impl : private base::NonCopyable
    {
      friend std::ostream & operator<<( std::ostream & str, const Impl & obj );
      friend std::ostream & dumpOn( std::ostream & str, const Impl & obj );

      /// <%posttrans script basename, pkgname> pairs.
      using ScriptList = std::list< std::pair<std::string,std::string> >;

      /// Data regarding the dumpfile used if `rpm --runposttrans` is supported
      struct Dumpfile
      {
        Dumpfile( Pathname dumpfile_r )
        : _dumpfile { std::move(dumpfile_r) }
        {}

        Pathname _dumpfile;         ///< The file holding the collected dump_posttrans: lines.
        size_t _numscripts = 0;     ///< Number of scripts we collected (roughly estimated)
        bool _runposttrans = true;  ///< Set to false if rpm lost --runposttrans support during transaction.
      };

      public:
        Impl( const Pathname & root_r )
        : _root( root_r )
        , _myJobReport { "cmdout", "%posttrans" }
        {}

        ~Impl()
        {}

        bool hasPosttransScript( const Pathname & rpmPackage_r )
        { return bool(getHeaderIfPosttrans( rpmPackage_r )); }

        void collectPosttransInfo( const Pathname & rpmPackage_r, const std::vector<std::string> & runposttrans_r )
        { if ( not collectDumpPosttransLines( runposttrans_r ) ) collectScriptForPackage( rpmPackage_r ); }

        void collectPosttransInfo( const std::vector<std::string> & runposttrans_r )
        { collectDumpPosttransLines( runposttrans_r ); }

        void collectScriptFromHeader( rpm::RpmHeader::constPtr pkg )
        {
          if ( pkg ) {
            if ( not _scripts ) {
              _scripts = ScriptList();
            }

            filesystem::TmpFile script( tmpDir(), pkg->ident() );
            filesystem::addmod( script.path(), 0500 );  // script must be executable
            script.autoCleanup( false );                // no autodelete; within a tmpdir
            {
              std::ofstream out( script.path().c_str() );
              out << "#! " << pkg->tag_posttransprog() << endl
                  << pkg->tag_posttrans() << endl;
            }

            _scripts->push_back( std::make_pair( script.path().basename(), pkg->tag_name() ) );
            MIL << "COLLECT posttrans: '" << PathInfo( script.path() ) << "' for package: '" << pkg->tag_name() << "'" << endl;
          }
        }

        void collectScriptForPackage( const Pathname & rpmPackage_r )
        { collectScriptFromHeader( getHeaderIfPosttrans( rpmPackage_r ) ); }

        /** Return whether runposttrans lines were collected.
         * If runposttrans is supported, rpm issues at least one 'dump_posttrans: enabled' line with
         * every install/remove. If no line is issued, rpm does not support --runposttrans. Interesting
         * for \ref executeScripts is whether the final call to rpm supported it.
         */
        bool collectDumpPosttransLines( const std::vector<std::string> & runposttrans_r )
        {
          if ( runposttrans_r.empty()  ) {
            if ( _dumpfile and _dumpfile->_runposttrans ) {
              MIL << "LOST dump_posttrans support" <<  endl;
              _dumpfile->_runposttrans = false;  // rpm was downgraded to a version not supporing --runposttrans
            }
            return false;
          }

          if ( not _dumpfile ) {
            filesystem::TmpFile dumpfile( tmpDir(), "dumpfile" );
            filesystem::addmod( dumpfile.path(), 0400 );  // dumpfile must be readable
            dumpfile.autoCleanup( false );	// no autodelete; within a tmpdir
            _dumpfile = Dumpfile( dumpfile.path() );
            MIL << "COLLECT dump_posttrans to '" << _dumpfile->_dumpfile << endl;
          }

          std::ofstream out( _dumpfile->_dumpfile.c_str(), std::ios_base::app );
          for ( const auto & s : runposttrans_r ) {
            out << s << endl;
          }
          _dumpfile->_numscripts += runposttrans_r.size();
          MIL << "COLLECT " << runposttrans_r.size() << " dump_posttrans lines" << endl;
          return true;
        }

        /** Execute the remembered scripts.
         * Crucial is the mixed case, where scripts and dumpfile are present at the end.
         * If rpm was updated in the transaction to a version supporting --runposttrans,
         * run scripts and then the dumpfile.
         * If rpm was downgraded in the transaction to a version no longer supporting
         * --runposttrans, we need to extract missing %posttrans scripts from the dumpfile
         * and execute them before the collected scripts.
         */
        void executeScripts( rpm::RpmDb & rpm_r )
        {
          if ( _dumpfile && not _dumpfile->_runposttrans ) {
            // Here a downgraded rpm lost the ability to --runposttrans. Extract at least any
            // missing %posttrans scripts collected in _dumpfile and prepend them to the _scripts.
            MIL << "Extract missing %posttrans scripts and prepend them to the scripts." << endl;

            // collectScriptFromHeader appends to _scripts, so we save here and append again later
            std::optional<ScriptList> savedscripts;
            if ( _scripts ) {
              savedscripts = std::move(*_scripts);
              _scripts = std::nullopt;
            }

            rpm::librpmDb::db_const_iterator it;
            recallFromDumpfile( _dumpfile->_dumpfile, [&]( std::string n_r, std::string v_r, std::string r_r, std::string a_r ) -> void {
              if ( it.findPackage( n_r, Edition( v_r, r_r ) ) && headerHasPosttrans( *it ) )
                collectScriptFromHeader( *it );
            } );

            // append any savedscripts
            if ( savedscripts ) {
              if ( _scripts ) {
                _scripts->splice( _scripts->end(), *savedscripts );
              } else {
                _scripts = std::move(*savedscripts);
              }
            }
            _dumpfile = std::nullopt;
          }

          if ( not ( _scripts || _dumpfile ) )
            return;  // Nothing todo

          // ProgressReport counting the scripts ( 0:preparation, 1->n:for n scripts, n+1: indicate success)
          callback::SendReport<ProgressReport> report;
          ProgressData scriptProgress( [&]() -> ProgressData::value_type {
            ProgressData::value_type ret = 1;
            if ( _scripts )
              ret += _scripts->size();
            if ( _dumpfile )
              ret += _dumpfile->_numscripts;
            return ret;
          }() );
          scriptProgress.sendTo( ProgressReportAdaptor( ProgressData::ReceiverFnc(), report ) );
          // Translator: progress bar label
          std::string scriptProgressName { _("Running post-transaction scripts") };
          // Translator: progress bar label; %1% is a script identifier like '%posttrans(mypackage-2-0.noarch)'
          str::Format fmtScriptProgressRun { _("Running %1% script") };
          // Translator: headline; %1% is a script identifier like '%posttrans(mypackage-2-0.noarch)'
          str::Format fmtRipoff { _("%1% script output:") };
          std::string sendRipoff;

          HistoryLog historylog;

          // lambda to prepare reports for a new script
          auto startNewScript = [&] ( const std::string & scriptident_r ) -> void {
            // scriptident_r : script identifier like "%transfiletriggerpostun(istrigger-2-0.noarch)"
            sendRipoff = fmtRipoff % scriptident_r;
            scriptProgress.name( fmtScriptProgressRun % scriptident_r );
            scriptProgress.incr();
          };

          // lambda to send script output to reports
          auto sendScriptOutput = [&] ( const std::string & line_r ) -> void {
            OnScopeExit cleanup;  // in case we need it
            if ( not sendRipoff.empty() ) {
              historylog.comment( sendRipoff, true /*timestamp*/);
              _myJobReport.set( "ripoff", std::cref(sendRipoff) );
              cleanup.setDispose( [&]() -> void {
                _myJobReport.erase( "ripoff" );
                sendRipoff.clear();
              } );
            }
            historylog.comment( line_r );
            _myJobReport.info( line_r );
          };

          // send the initial progress report
          scriptProgress.name( scriptProgressName );
          scriptProgress.toMin();

          // Scripts first...
          if ( _scripts ) {
            Pathname noRootScriptDir( ZConfig::instance().update_scriptsPath() / tmpDir().basename() );
            // like rpm would report it (intentionally not translated and NL-terminated):
            str::Format fmtScriptFailedMsg { "warning: %%posttrans(%1%) scriptlet failed, exit status %2%\n" };
            str::Format fmtPosttrans { "%%posttrans(%1%)" };

            rpm::librpmDb::db_const_iterator it;  // Open DB only once
            while ( ! _scripts->empty() )
            {
              const auto &scriptPair = _scripts->front();
              const std::string & script = scriptPair.first;
              const std::string & pkgident( script.substr( 0, script.size()-6 ) ); // strip tmp file suffix[6]
              startNewScript( fmtPosttrans % pkgident );

              int npkgs = 0;
              for ( it.findByName( scriptPair.second ); *it; ++it )
                ++npkgs;

              MIL << "EXECUTE posttrans: " << script << " with argument: " << npkgs << endl;
              ExternalProgram::Arguments cmd {
                "/bin/sh",
                (noRootScriptDir/script).asString(),
                str::numstring( npkgs )
              };
              ExternalProgram prog( cmd, ExternalProgram::Stderr_To_Stdout, false, -1, true, _root );

              for( std::string line = prog.receiveLine(); ! line.empty(); line = prog.receiveLine() ) {
                sendScriptOutput( line );
              }
              //script was executed, remove it from the list
              _scripts->pop_front();

              int ret = prog.close();
              if ( ret != 0 )
              {
                std::string msg { fmtScriptFailedMsg % pkgident % ret };
                WAR << msg;
                sendScriptOutput( msg ); // info!, as rpm would have reported it.
              }
            }
            _scripts = std::nullopt;
          }

          // ...then 'rpm --runposttrans'
          int res = 0;  // Indicate a failed call to rpm itself! (a failed script is just a warning)
          if ( _dumpfile ) {
            res = rpm_r.runposttrans( _dumpfile->_dumpfile, [&] ( const std::string & line_r ) ->void {
              if ( str::startsWith( line_r, "RIPOFF:" ) )
                startNewScript( line_r.substr( 7 ) ); // new scripts ident sent by rpm
              else
                sendScriptOutput( line_r );
            } );
            if ( res != 0 )
              _myJobReport.error( str::Format("rpm --runposttrans returned %1%.") % res );

            _dumpfile = std::nullopt;
          }

          // send a final progress report
          scriptProgress.name( scriptProgressName );
          if ( res == 0 )
            scriptProgress.toMax(); // Indicate 100%, in case Dumpfile::_numscripts estimation was off
          return;
        }

        /** Discard all remembered scrips.
         * As we are just logging the omitted actions, we don't pay further attention
         * to the mixed case, where scripts and dumpfile are present (see executeScripts).
         */
        void discardScripts()
        {
          if ( not ( _scripts || _dumpfile ) )
            return;  // Nothing todo

          str::Str msg;

          if ( _scripts ) {
            // Legacy format logs all collected %posttrans
            msg << "%posttrans scripts skipped while aborting:" << endl;
            for ( const auto & script : *_scripts )
            {
              WAR << "UNEXECUTED posttrans: " << script.first << endl;
              const std::string & pkgident( script.first.substr( 0, script.first.size()-6 ) ); // strip tmp file suffix[6]
              msg << "    " << pkgident << "\n";
            }
            _scripts = std::nullopt;
          }

          if ( _dumpfile ) {
            msg << "%posttrans and %transfiletrigger scripts are not executed when aborting!" << endl;
            _dumpfile = std::nullopt;
          }

          HistoryLog historylog;
          historylog.comment( msg, true /*timestamp*/);
          _myJobReport.warning( msg );
        }

      private:
        /** Lazy create tmpdir on demand. */
        Pathname tmpDir()
        {
          if ( !_ptrTmpdir ) _ptrTmpdir.reset( new filesystem::TmpDir( _root / ZConfig::instance().update_scriptsPath(), "posttrans" ) );
          DBG << _ptrTmpdir->path() << endl;
          return _ptrTmpdir->path();
        }

        /** Return whether RpmHeader has a  %posttrans. */
        bool headerHasPosttrans( rpm::RpmHeader::constPtr pkg_r ) const
        {
          bool ret = false;
          if ( pkg_r ) {
            std::string prog( pkg_r->tag_posttransprog() );
            if ( not prog.empty() && prog != "<lua>" )  // by now leave lua to rpm
              ret = true;
          }
          return ret;
        }

        /** Cache RpmHeader for consecutive hasPosttransScript / collectScriptForPackage calls.
         * @return RpmHeader::constPtr IFF \a rpmPackage_r has a %posttrans
         */
        rpm::RpmHeader::constPtr getHeaderIfPosttrans( const Pathname & rpmPackage_r )
        {
          if ( _headercache.first == rpmPackage_r )
            return _headercache.second;

          rpm::RpmHeader::constPtr ret { rpm::RpmHeader::readPackage( rpmPackage_r, rpm::RpmHeader::NOVERIFY ) };
          if ( ret ) {
            if ( not headerHasPosttrans( ret ) )
              ret = nullptr;
          } else {
            WAR << "Unexpectedly this is no package: " << rpmPackage_r << endl;
          }
          _headercache = std::make_pair( rpmPackage_r, ret );
          return ret;
        }

        /** Retrieve "dump_posttrans: install" lines from \a dumpfile_r and pass n,v,r,a to the \a consumer_r. */
        void recallFromDumpfile( const Pathname & dumpfile_r, std::function<void(std::string,std::string,std::string,std::string)> consume_r )
        {
          // dump_posttrans: install 10 terminfo-base-6.4.20230819-19.1.x86_64
          static const str::regex rxInstalled { "^dump_posttrans: +install +[0-9]+ +(.+)-([^-]+)-([^-]+)\\.([^.]+)" };
          str::smatch what;
          iostr::forEachLine( InputStream( dumpfile_r ), [&]( int num_r, std::string line_r ) -> bool {
            if( str::regex_match( line_r, what, rxInstalled ) )
              consume_r( what[1], what[2], what[3], what[4] );
            return true; // continue iostr::forEachLine
          } );
        }

      private:
        Pathname _root;
        std::optional<ScriptList> _scripts;
        std::optional<Dumpfile> _dumpfile;
        boost::scoped_ptr<filesystem::TmpDir> _ptrTmpdir;

        UserDataJobReport _myJobReport;       ///< JobReport with ContentType "cmdout/%posttrans"

        std::pair<Pathname,rpm::RpmHeader::constPtr> _headercache;
    };

    /** \relates RpmPostTransCollector::Impl Stream output */
    inline std::ostream & operator<<( std::ostream & str, const RpmPostTransCollector::Impl & obj )
    { return str << "RpmPostTransCollector::Impl"; }

    /** \relates RpmPostTransCollector::Impl Verbose stream output */
    inline std::ostream & dumpOn( std::ostream & str, const RpmPostTransCollector::Impl & obj )
    { return str << obj; }

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : RpmPostTransCollector
    //
    ///////////////////////////////////////////////////////////////////

    RpmPostTransCollector::RpmPostTransCollector( const Pathname & root_r )
      : _pimpl( new Impl( root_r ) )
    {}

    RpmPostTransCollector::~RpmPostTransCollector()
    {}

    bool RpmPostTransCollector::hasPosttransScript( const Pathname & rpmPackage_r )
    { return _pimpl->hasPosttransScript( rpmPackage_r ); }

    void RpmPostTransCollector::collectPosttransInfo( const Pathname & rpmPackage_r, const std::vector<std::string> & runposttrans_r )
    { _pimpl->collectPosttransInfo( rpmPackage_r, runposttrans_r ); }

    void RpmPostTransCollector::collectPosttransInfo( const std::vector<std::string> & runposttrans_r )
    { _pimpl->collectPosttransInfo( runposttrans_r ); }

    void RpmPostTransCollector::executeScripts( rpm::RpmDb & rpm_r )
    { _pimpl->executeScripts( rpm_r ); }

    void RpmPostTransCollector::discardScripts()
    { return _pimpl->discardScripts(); }

    std::ostream & operator<<( std::ostream & str, const RpmPostTransCollector & obj )
    { return str << *obj._pimpl; }

    std::ostream & dumpOn( std::ostream & str, const RpmPostTransCollector & obj )
    { return dumpOn( str, *obj._pimpl ); }

  } // namespace target
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
