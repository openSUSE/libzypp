/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ERROR_P_H
#define ERROR_P_H

#include <zypp-glib/error.h>
#include <exception>

void zypp_error_from_exception (GError **err, std::exception_ptr exception );

#endif
