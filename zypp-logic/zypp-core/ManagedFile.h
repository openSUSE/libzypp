/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ManagedFile.h
 *
*/
#ifndef ZYPP_MANAGEDFILE_H
#define ZYPP_MANAGEDFILE_H

#include <iosfwd>

#include <zypp/Pathname.h>
#include <zypp/AutoDispose.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  /** A Pathname plus associated cleanup code to be executed when
   *  path is no longer needed.
   */
  using ManagedFile = AutoDispose<const Pathname>;

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_MANAGEDFILE_H
