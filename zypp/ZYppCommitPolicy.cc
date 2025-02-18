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
#include <zypp/ZYppCommitPolicy.h>
#include <zypp-core/base/LogControl.h>
#include <zypp-core/TriBool.h>
#include <zypp/PathInfo.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/ZYppFactory.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  inline bool ImZYPPER()
  {
    static bool ret = filesystem::readlink( "/proc/self/exe" ).basename() == "zypper";
    return ret;
  }

  inline bool singleTransInEnv()
  {
    const char *val = ::getenv("ZYPP_SINGLE_RPMTRANS");
    return ( val && std::string_view( val ) == "1" );
  }

  inline bool singleTransEnabled()
  {
#ifdef SINGLE_RPMTRANS_AS_DEFAULT_FOR_ZYPPER
    return ImZYPPER();
#else // SINGLE_RPMTRANS_AS_DEFAULT_FOR_ZYPPER
    return ImZYPPER() && singleTransInEnv();
#endif // SINGLE_RPMTRANS_AS_DEFAULT_FOR_ZYPPER
  }


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
      mutable bool              _singleTransMode; // mutable: [bsc#1189788] pending usrmerge must disable singletrans

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
  {
    if ( singleTransModeEnabled() && _pimpl->_downloadMode == DownloadAsNeeded ) {
      DBG << _pimpl->_downloadMode << " is not compatible with singleTransMode, falling back to " << DownloadInAdvance << std::endl;
      return DownloadInAdvance;
    }
    return _pimpl->_downloadMode;
  }

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
    if ( _pimpl->_singleTransMode and pendingUsrmerge() ) {
      WAR << "Ignore $ZYPP_SINGLE_RPMTRANS=1: Bug 1189788 - UsrMerge: filesystem package breaks system when upgraded in a single rpm transaction" << std::endl;
      JobReport::info(
        "[bsc#1189788] The filesystem package seems to be unable to perform the pending\n"
        "              UsrMerge reliably in a single transaction. The single_rpmtrans\n"
        "              backend will therefore be IGNORED and the transaction is performed\n"
        "              by the classic_rpmtrans backend."
        , JobReport::UserData( "cmdout", "[bsc#1189788]" ) );
      _pimpl->_singleTransMode = false;
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
