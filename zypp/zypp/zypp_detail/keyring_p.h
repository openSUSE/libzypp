/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp-common/private/keyring_p.h
 *
*/
#ifndef ZYPP_ZYPP_DETAIL_KEYRINGIMPL_H
#define ZYPP_ZYPP_DETAIL_KEYRINGIMPL_H

#include <zypp/KeyRing.h>
#include <zypp-common/private/keyring_p.h>

namespace zypp {

  struct KeyRing::Impl : public KeyRingImpl
  {
    Impl(const filesystem::Pathname &baseTmpDir);
  };

}

#endif
