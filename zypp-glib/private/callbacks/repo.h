/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZMART_SOURCE_CALLBACKS_H
#define ZMART_SOURCE_CALLBACKS_H

#include <sstream>

#include <zypp/base/Logger.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/Pathname.h>
#include <zypp/Url.h>
#include <zypp/target/rpm/RpmDb.h>

#include <zypp-glib/private/callbacks/legacyreportreceiverbase.h>
#include <zypp-core/zyppng/pipelines/expected.h>
#include <zypp-core/base/Gettext.h>

///////////////////////////////////////////////////////////////////
namespace zyppng
{
///////////////////////////////////////////////////////////////////

// progress for downloading a resolvable
struct DownloadResolvableReportReceiver : public zypp::callback::ReceiveReport<zypp::repo::DownloadResolvableReport>
{
private:
  LegacyReportReceiverBase &_parent;
  zyppng::ProgressObserverRef _progress;
public:

  DownloadResolvableReportReceiver( LegacyReportReceiverBase &parent ) : _parent( parent ) {}

  zypp::Resolvable::constPtr _resolvable_ptr;
  zypp::Url _url;
  zypp::Pathname _delta;
  zypp::ByteCount _delta_size;
  std::string _label_apply_delta;
  zypp::Pathname _patch;
  zypp::ByteCount _patch_size;

  void reportbegin() override
  {

    //legacy code instantiated a SendReport
  }
  void reportend() override
  {
    //legacy code deleted a SendReport
  }

  // Dowmload delta rpm:
  // - path below url reported on start()
  // - expected download size (0 if unknown)
  // - download is interruptable
  // - problems are just informal
  void startDeltaDownload( const zypp::Pathname & filename, const zypp::ByteCount & downloadsize ) override
  {
    if ( _parent.masterProgress() ) {

      if ( _progress )
        ERR << "Exisiting progress in DownloadResolvableReportReceiver, this is a bug" << std::endl;

      _delta = filename;
      _delta_size = downloadsize;
      std::ostringstream s;
      s << _("Retrieving delta") << ": " << _delta;

      _progress = _parent.masterProgress ()->makeSubTask ( 1.0, s.str(), downloadsize );
      _progress->start();
    }
  }

  bool progressDeltaDownload( int value ) override {
    if ( _progress ) {
      _progress->setCurrent ( value );
    }
    return true;
  }

  void problemDeltaDownload( const std::string & description ) override
  {
    if ( _progress ) {
      _progress->setLabel ( description );
      _progress->setFinished ( ProgressObserver::Error );
      _progress.reset();
    }
  }


  void finishDeltaDownload() override
  {
    if ( _progress ) {
      _progress->setFinished();
      _progress.reset();
    }
  }

  // Apply delta rpm:
  // - local path of downloaded delta
  // - aplpy is not interruptable
  // - problems are just informal
  void startDeltaApply( const zypp::Pathname & filename ) override
  {
    if ( _parent.masterProgress() ) {

      if ( _progress )
        ERR << "Exisiting progress in DownloadResolvableReportReceiver, this is a bug" << std::endl;

      std::ostringstream s;
      // translators: this text is a progress display label e.g. "Applying delta foo [42%]"
      s << _("Applying delta") << ": " << _delta;
      _label_apply_delta = s.str();
      _delta = filename;

      _progress = _parent.masterProgress ()->makeSubTask ( 1.0, _label_apply_delta );
      _progress->start();
    }
    _delta = filename.basename();

  }

  void progressDeltaApply( int value ) override
  {
    if ( _progress ) {
      _progress->setCurrent ( value );
    }
  }

  void problemDeltaApply( const std::string & description ) override
  {
    if ( _progress ) {
      _progress->setLabel ( description );
      _progress->setFinished ( ProgressObserver::Error );
      _progress.reset();
    }
  }

  void finishDeltaApply() override
  {
    if ( _progress ) {
      _progress->setFinished();
      _progress.reset();
    }
  }

  void fillsRhs( TermLine & outstr_r, Zypper & zypper_r, Resolvable::constPtr res_r )
  {
    outstr_r.rhs << " (" << ++zypper_r.runtimeData().commit_pkg_current
                 << "/" << zypper_r.runtimeData().commit_pkgs_total << ")";
    if ( res_r )
    {
      outstr_r.rhs << ", " << res_r->downloadSize().asString( 5, 3 ) << "    "/* indent `/s)]` of download speed line following */;
      // TranslatorExplanation %s is package size like "5.6 M"
      // outstr_r.rhs << " " << str::Format(_("(%s unpacked)")) % pkg_r->installSize().asString( 5, 3 );
    }
  }

  virtual void infoInCache( Resolvable::constPtr res_r, const Pathname & localfile_r )
  {
    Zypper & zypper = Zypper::instance();

    TermLine outstr( TermLine::SF_SPLIT | TermLine::SF_EXPAND );
    outstr.lhs << str::Format(_("In cache %1%")) % localfile_r.basename();
    fillsRhs( outstr, zypper, res_r );
    zypper.out().infoLine( outstr );
  }

  /** this is interesting because we have full resolvable data at hand here
   * The media backend has only the file URI
   * \todo combine this and the media data progress callbacks in a reasonable manner
   */
  virtual void start( Resolvable::constPtr resolvable_ptr, const Url & url )
  {
    _resolvable_ptr =  resolvable_ptr;
    _url = url;
    Zypper & zypper = Zypper::instance();

    TermLine outstr( TermLine::SF_SPLIT | TermLine::SF_EXPAND );
    outstr.lhs << _("Retrieving:") << " " << _resolvable_ptr-> asUserString();
    fillsRhs( outstr, zypper, resolvable_ptr );

    // temporary fix for bnc #545295
    if ( zypper.runtimeData().commit_pkg_current == zypper.runtimeData().commit_pkgs_total )
      zypper.runtimeData().commit_pkg_current = 0;

    zypper.out().infoLine( outstr );
    zypper.runtimeData().action_rpm_download = true;
  }

  // The progress is reported by the media backend
  // virtual bool progress(int value, Resolvable::constPtr /*resolvable_ptr*/) { return true; }

  virtual Action problem( Resolvable::constPtr resolvable_ptr, Error /*error*/, const std::string & description )
  {
    Zypper::instance().out().error(description);
    DBG << "error report" << std::endl;

    Action action = (Action) read_action_ari(PROMPT_ARI_RPM_DOWNLOAD_PROBLEM, ABORT);
    if (action == DownloadResolvableReport::RETRY)
      --Zypper::instance().runtimeData().commit_pkg_current;
    else
      Zypper::instance().runtimeData().action_rpm_download = false;
    return action;
  }

  virtual void pkgGpgCheck( const UserData & userData_r )
  {
    Zypper & zypper = Zypper::instance();
    // "ResObject"		ResObject::constPtr of the package
    // "Localpath"		Pathname to downloaded package on disk
    // "CheckPackageResult"	RpmDb::CheckPackageResult of signature check
    // "CheckPackageDetail"	RpmDb::CheckPackageDetail logmessages of rpm signature check
    //
    //   Userdata accepted:
    // "Action"			DownloadResolvableReport::Action user advice how to behave on error (ABORT).
    using target::rpm::RpmDb;
    RpmDb::CheckPackageResult result		( userData_r.get<RpmDb::CheckPackageResult>( "CheckPackageResult" ) );
    const RpmDb::CheckPackageDetail & details	( userData_r.get<RpmDb::CheckPackageDetail>( "CheckPackageDetail" ) );

    str::Str msg;
    if ( result != RpmDb::CHK_OK )	// only on error...
    {
      const Pathname & localpath		( userData_r.get<Pathname>( "Localpath" ) );
      const std::string & rpmname		( localpath.basename() );
      msg << rpmname << ":" << "\n";
    }

    // report problems in individual checks...
    for ( const auto & el : details )
    {
      switch ( el.first )
      {
        case RpmDb::CHK_OK:
          if ( zypper.out().verbosity() >= Out::HIGH )	// quiet about good sigcheck unless verbose.
            msg << el.second << "\n";
          break;
        case RpmDb::CHK_NOSIG:
          msg << ( (result == RpmDb::CHK_OK ? ColorContext::MSG_WARNING : ColorContext::MSG_ERROR ) << el.second ) << "\n";
          break;
        case RpmDb::CHK_NOKEY:		// can't check
        case RpmDb::CHK_NOTFOUND:
          msg << ( ColorContext::MSG_WARNING << el.second ) << "\n";
          break;
        case RpmDb::CHK_FAIL:		// failed check
        case RpmDb::CHK_NOTTRUSTED:
        case RpmDb::CHK_ERROR:
          msg << ( ColorContext::MSG_ERROR << el.second ) << "\n";
          break;
      }
    }

    if ( result == RpmDb::CHK_OK )
    {
      const std::string & msgstr( msg.str() );
      if ( ! msgstr.empty() )
        zypper.out().info( msg );
      return;
    }

    // determine action
    if ( zypper.config().no_gpg_checks )
    {
      // report and continue
      ResObject::constPtr pkg( userData_r.get<ResObject::constPtr>( "ResObject" ) );
      std::string err( str::Str() << pkg->asUserString() << ": " << _("Signature verification failed") << " " << result );
      switch ( result )
      {
        case RpmDb::CHK_OK:
          // Can't happen; already handled above
          break;

        case RpmDb::CHK_NOKEY:		// can't check
        case RpmDb::CHK_NOTFOUND:
        case RpmDb::CHK_NOSIG:
          msg << ( ColorContext::MSG_WARNING << err ) << "\n";
          break;

        case RpmDb::CHK_FAIL:		// failed check
        case RpmDb::CHK_ERROR:
        case RpmDb::CHK_NOTTRUSTED:
        default:
          msg << ( ColorContext::MSG_ERROR << err ) << "\n";
          break;
      }
      msg << _("Accepting package despite the error.") << " (--no-gpg-checks)" << "\n";
      userData_r.set( "Action", IGNORE );
    }
    else
    {
      // error -> requests the default problem report
      userData_r.reset( "Action" );
    }
    zypper.out().info( msg );
  }

  // implementation not needed prehaps - the media backend reports the download progress
  virtual void finish( Resolvable::constPtr /*resolvable_ptr**/, Error error, const std::string & reason )
  {
    Zypper::instance().runtimeData().action_rpm_download = false;
    /*
    display_done ("download-resolvable", cout_v);
    display_error (error, reason);
*/
  }
};

struct ProgressReportReceiver  : public zypp::callback::ReceiveReport<zypp::ProgressReport>
{
private:
  LegacyReportReceiverBase &_parent;
  zyppng::ProgressObserverRef _progress;
public:

  ProgressReportReceiver( LegacyReportReceiverBase &parent ) : _parent( parent ) {}
  virtual void start( const zypp::ProgressData &data )
  {
    if ( _parent.masterProgress() ) {
      _progress = _parent.masterProgress ()->makeSubTask ( 1.0, data.name(), data.max () - data.min () );
      _progress->start();
      _progress->setCurrent ( data.val () - data.min () );
    }
  }

  virtual bool progress( const zypp::ProgressData &data )
  {
    if ( _progress ) {
      _progress->setBaseSteps ( data.max () - data.min () );
      _progress->setCurrent ( data.val () - data.min () );
      _progress->setLabel ( data.name () );
    }
  }

  virtual void finish( const zypp::ProgressData &data )
  {
    if ( _progress ) {
      // Don't report success if data reports percent and is not at 100%
      zypp::ProgressData::value_type last = data.reportValue();

      _progress->setBaseSteps ( data.max () - data.min () );
      _progress->setCurrent ( data.val () - data.min () );
      _progress->setLabel ( data.name () );
      _progress->setFinished ( (last == 100) ? zyppng::ProgressObserver::Success : zyppng::ProgressObserver::Error );
      _progress.reset();
    }
  }
};

}
#endif
