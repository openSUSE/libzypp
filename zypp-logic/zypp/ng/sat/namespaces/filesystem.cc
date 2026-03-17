/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "filesystem.h"

#include <zypp-core/parser/sysconfig.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/LogTools.h>
#include <zypp-core/fs/watchfile.h>

#include <zypp/ng/sat/pool.h>

namespace zyppng::sat::namespaces {

  namespace {
    const zypp::Pathname & sysconfigStoragePath()
    {
      static const zypp::Pathname _val( "/etc/sysconfig/storage" );
      return _val;
    }
  }

  bool FilesystemNamespaceProvider::isSatisfied(detail::IdType value ) const
  {
    const auto &req = requiredFilesystems ();
    return req.count( zypp::IdString(value).asString() ) > 0;
  }

  void FilesystemNamespaceProvider::checkDirty( Pool & pool )
  {
    static zypp::WatchFile sysconfigFile( sysconfigStoragePath(), zypp::WatchFile::NO_INIT );
    if ( _requiredFilesystems && sysconfigFile.hasChanged() ) {
      _requiredFilesystems.reset(); // recreated on demand
      pool.setDirty( PoolInvalidation::Data, { "/etc/sysconfig/storage change" } );
    }
  }

  const std::set<std::string> & FilesystemNamespaceProvider::requiredFilesystems() const
  {
    if ( !_requiredFilesystems )
    {
      _requiredFilesystems = std::set<std::string>();
      std::set<std::string> & requiredFilesystems( *_requiredFilesystems );
      zypp::str::split( zypp::base::sysconfig::read( sysconfigStoragePath() )["USED_FS_LIST"],
                  std::inserter( requiredFilesystems, requiredFilesystems.end() ) );
    }
    return *_requiredFilesystems;
  }


} // namespace zyppng::sat
