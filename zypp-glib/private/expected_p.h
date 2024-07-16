/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_GLIB_PRIVATE_EXPECTED_P_H
#define ZYPP_GLIB_PRIVATE_EXPECTED_P_H

#include <zypp-glib/expected.h>
struct _ZyppExpected {
  GObject parent_instance;

  GError *error;
  GValue value;
};

#endif
