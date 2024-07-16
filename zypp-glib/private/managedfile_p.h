/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_PRIVATE_MANAGEDFILE_P_H
#define ZYPP_GLIB_PRIVATE_MANAGEDFILE_P_H

#include <zypp-glib/managedfile.h>
#include <zypp/ManagedFile.h>

struct _ZyppManagedFile {
  zypp::ManagedFile _file;
};

ZyppManagedFile *zypp_managed_file_new( const zypp::ManagedFile &f );

#endif
