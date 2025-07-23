/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ZYppCommitPolicy.cc
 *
*/

#include <iostream>

#include <zypp-core/base/String.h>
#include <zypp-core/base/StringV.h>

#include <zypp/ZConfig.h>
#include <zypp-core/APIConfig.h>
#include <zypp/ZYppCommitPolicy.h>
#include <zypp-core/base/LogControl.h>
#include <zypp-core/TriBool.h>
#include <zypp/PathInfo.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/ZYppFactory.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
#ifdef SUSE

#if APIConfig(LIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT)
  // In old codebases it can be explicitly enabled for zypper only via ZYPP_SINGLE_RPMTRANS.
  inline bool ImZYPPER()
  {
    static bool ret = filesystem::readlink( "/proc/self/exe" ).basename() == "zypper";
    return ret;
  }
  inline bool singleTransInEnv()
  {
    const char *val = ::getenv("ZYPP_SINGLE_RPMTRANS");
    return ( val && str::strToFalse( val ) );
  }
  inline bool singleTransEnabled()
  { return ImZYPPER() && singleTransInEnv(); }
#else
  // SUSE using singletrans as default may allow to explicitly disable it via ZYPP_SINGLE_RPMTRANS.
  // NOTE: singleTransInEnv() here is no real enablement because it defaults to false
  //       if ZYPP_SINGLE_RPMTRANS is undefined. By now it just allows using singletrans
  //       with all applications, not just zypper. In case it actually becomes the
  //       default on SUSE, we may just want a method to explicitly disable it.
  inline bool singleTransInEnv()
  {
    const char *val = ::getenv("ZYPP_SINGLE_RPMTRANS");
    return ( val && str::strToFalse( val ) );
  }
  inline bool singleTransEnabled()
  { return singleTransInEnv(); }
#endif

#else  // not SUSE
  inline bool singleTransEnabled()
  { return true; }
#endif
///////////////////////////////////////////////////////////////////

  inline bool isPreUsrmerge( const Pathname & root_r )
  {
    // If systems /lib is a directory and not a symlink.
    return PathInfo( root_r / "lib", PathInfo::LSTAT ).isDir();
  }

  inline bool transactionWillUsrmerge()
  {
    // A package providing "may-perform-usrmerge" is going to be installed.
    const sat::WhatProvides q { Capability("may-perform-usrmerge") };
    for ( const auto & pi : q.poolItem() ) {
      if ( pi.status().isToBeInstalled() )
        return true;
    }
    return false;
  }

  inline bool pendingUsrmerge()
  {
#if 1
    // introduced solely for the use in QA test for Code16
    const char *val = ::getenv("ZYPP_NO_USRMERGE_PROTECT");
    if ( val && str::strToTrue( val ) )
      return false;
#endif

    // NOTE: Bug 1189788 - UsrMerge: filesystem package breaks system when
    // upgraded in a single rpm transaction. Target::Commit must amend this
    // request depending on whether a UsrMerge may be performed by the
    // transaction.
    Target_Ptr target { getZYpp()->getTarget() };
    bool ret = target && isPreUsrmerge( target->root() ) && transactionWillUsrmerge();
    return ret;
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZYppCommitPolicy::Impl
  //
  ///////////////////////////////////////////////////////////////////

  class ZYppCommitPolicy::Impl
  {
    public:
      Impl()
      : _restrictToMedia	( 0 )
      , _downloadMode		( ZConfig::instance().commit_downloadMode() )
      , _rpmInstFlags		( ZConfig::instance().rpmInstallFlags() )
      , _syncPoolAfterCommit	( true )
      , _singleTransMode        ( singleTransEnabled() )
      {}

    public:
      unsigned			_restrictToMedia;
      DownloadMode		_downloadMode;
      target::rpm::RpmInstFlags	_rpmInstFlags;
      bool			_syncPoolAfterCommit;
      bool                      _singleTransMode;

      mutable bool _notifyBSC1189788 = true; // mutable: send notification just once

    private:
      friend Impl * rwcowClone<Impl>( const Impl * rhs );
      /** clone for RWCOW_pointer */
      Impl * clone() const { return new Impl( *this ); }
  };

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZYppCommitPolicy
  //
  ///////////////////////////////////////////////////////////////////

  ZYppCommitPolicy::ZYppCommitPolicy()
  : _pimpl( new Impl )
  {}


  ZYppCommitPolicy & ZYppCommitPolicy::restrictToMedia( unsigned mediaNr_r )
  { _pimpl->_restrictToMedia = ( mediaNr_r == 1 ) ? 1 : 0; return *this; }

  unsigned ZYppCommitPolicy::restrictToMedia() const
  { return _pimpl->_restrictToMedia; }


  ZYppCommitPolicy & ZYppCommitPolicy::dryRun( bool yesNo_r )
  {  _pimpl->_rpmInstFlags.setFlag( target::rpm::RPMINST_TEST, yesNo_r ); return *this; }

  bool ZYppCommitPolicy::dryRun() const
  { return _pimpl->_rpmInstFlags.testFlag( target::rpm::RPMINST_TEST );}

  ZYppCommitPolicy & ZYppCommitPolicy::downloadMode( DownloadMode val_r )
  { _pimpl->_downloadMode = val_r; return *this; }

  DownloadMode ZYppCommitPolicy::downloadMode() const
  { return _pimpl->_downloadMode; }

  ZYppCommitPolicy &  ZYppCommitPolicy::rpmInstFlags( target::rpm::RpmInstFlags newFlags_r )
  { _pimpl->_rpmInstFlags = newFlags_r; return *this; }

  ZYppCommitPolicy & ZYppCommitPolicy::rpmNoSignature( bool yesNo_r )
  { _pimpl->_rpmInstFlags.setFlag( target::rpm::RPMINST_NOSIGNATURE, yesNo_r ); return *this; }

  ZYppCommitPolicy & ZYppCommitPolicy::rpmExcludeDocs( bool yesNo_r )
  { _pimpl->_rpmInstFlags.setFlag( target::rpm::RPMINST_EXCLUDEDOCS, yesNo_r ); return *this; }

  target::rpm::RpmInstFlags ZYppCommitPolicy::rpmInstFlags() const
  { return _pimpl->_rpmInstFlags; }

  bool ZYppCommitPolicy::rpmNoSignature() const
  { return _pimpl->_rpmInstFlags.testFlag( target::rpm::RPMINST_NOSIGNATURE ); }

  bool ZYppCommitPolicy::rpmExcludeDocs() const
  { return _pimpl->_rpmInstFlags.testFlag( target::rpm::RPMINST_EXCLUDEDOCS ); }

  ZYppCommitPolicy &ZYppCommitPolicy::allowDowngrade(bool yesNo_r)
  { _pimpl->_rpmInstFlags.setFlag( target::rpm::RPMINST_ALLOWDOWNGRADE, yesNo_r ); return *this; }

  bool ZYppCommitPolicy::allowDowngrade() const
  { return _pimpl->_rpmInstFlags.testFlag( target::rpm::RPMINST_ALLOWDOWNGRADE ); }

  ZYppCommitPolicy &ZYppCommitPolicy::replaceFiles( bool yesNo_r )
  { _pimpl->_rpmInstFlags.setFlag( target::rpm::RPMINST_REPLACEFILES, yesNo_r ); return *this; }

  bool ZYppCommitPolicy::replaceFiles( ) const
  { return _pimpl->_rpmInstFlags.testFlag( target::rpm::RPMINST_REPLACEFILES ); }

  ZYppCommitPolicy & ZYppCommitPolicy::syncPoolAfterCommit( bool yesNo_r )
  { _pimpl->_syncPoolAfterCommit = yesNo_r; return *this; }

  bool ZYppCommitPolicy::syncPoolAfterCommit() const
  { return _pimpl->_syncPoolAfterCommit; }

  bool ZYppCommitPolicy::singleTransModeEnabled() const
  {
    if ( _pimpl->_singleTransMode ) {
      // Explicitly selecting DownloadAsNeeded also selects the classic_rpmtrans backend.
      // NOTE: This behavior was negotiated with YAST/Agama and is required for
      // certain install scenarios. Don't change it without checking with the teams.
      if ( _pimpl->_downloadMode == DownloadAsNeeded ) {
        MIL << "DownloadAsNeeded enforces the classic_rpmtrans backend!" << std::endl;
        return false;
      }

      if ( pendingUsrmerge() ) {
        WAR << "Ignore $ZYPP_SINGLE_RPMTRANS=1: Bug 1189788 - UsrMerge: filesystem package breaks system when upgraded in a single rpm transaction" << std::endl;
        if ( _pimpl->_notifyBSC1189788 ) {
          _pimpl->_notifyBSC1189788 = false;
          JobReport::info(
            "[bsc#1189788] The filesystem package seems to be unable to perform the pending\n"
            "              UsrMerge reliably in a single transaction. The single_rpmtrans\n"
            "              backend will therefore be IGNORED and the transaction is performed\n"
            "              by the classic_rpmtrans backend."
            , JobReport::UserData( "cmdout", "[bsc#1189788]" ) );
        }
        return false;
      }
    }
    return _pimpl->_singleTransMode;
  }

  std::ostream & operator<<( std::ostream & str, const ZYppCommitPolicy & obj )
  {
    str << "CommitPolicy(";
    if ( obj.restrictToMedia() )
      str << " restrictToMedia:" << obj.restrictToMedia();
    if ( obj.dryRun() )
      str << " dryRun";
    str << " " << obj.downloadMode();
    if ( obj.syncPoolAfterCommit() )
      str << " syncPoolAfterCommit";
    if ( obj.rpmInstFlags() )
      str << " rpmInstFlags{" << str::hexstring(obj.rpmInstFlags()) << "}";
    return str << " )";
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
