/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ZYppCallbacks.cc
 *
*/
#include "ZYppCallbacks.h"

namespace zypp {

   const JobReport::ContentType JobReport::repoRefreshMirrorlist   { "reporefresh", "mirrorlist" };

   const callback::UserData::ContentType target::rpm::SingleTransReport::contentLogline { "zypp-rpm", "logline" };
   const callback::UserData::ContentType target::rpm::InstallResolvableReportSA::contentRpmout( "zypp-rpm","installpkgsa" );
   const callback::UserData::ContentType target::rpm::RemoveResolvableReportSA::contentRpmout( "zypp-rpm","removepkgsa" );
   const callback::UserData::ContentType target::rpm::CommitScriptReportSA::contentRpmout( "zypp-rpm","scriptsa" );
   const callback::UserData::ContentType target::rpm::TransactionReportSA::contentRpmout( "zypp-rpm","transactionsa" );
   const callback::UserData::ContentType target::rpm::CleanupPackageReportSA::contentRpmout( "zypp-rpm","cleanupkgsa" );


   const callback::UserData::ContentType target::rpm::InstallResolvableReport::contentRpmout( "rpmout","installpkg" );
   const callback::UserData::ContentType target::rpm::RemoveResolvableReport::contentRpmout( "rpmout","removepkg" );

}
