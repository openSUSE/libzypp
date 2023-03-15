/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPPNG_PRIVATE_EXPECTED_P_H
#define ZYPPNG_PRIVATE_EXPECTED_P_H

#include <zyppng/expected.h>
struct _ZyppExpected {
  GObject parent_instance;

  GError *error;
  GValue value;
};

#endif
