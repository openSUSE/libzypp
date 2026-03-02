/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPP_NG_SAT_NAMESPACES_FILESYSTEM_H_INCLUDED
#define ZYPP_NG_SAT_NAMESPACES_FILESYSTEM_H_INCLUDED

#include <zypp/ng/sat/namespaces/namespaceprovider.h>
#include <set>
#include <string>
#include <optional>

namespace zyppng::sat::namespaces {

  /**
   * \brief Provider for NAMESPACE_FILESYSTEM.
   * Checks for required filesystems (e.g., from /etc/sysconfig/storage).
   */
  class FilesystemNamespaceProvider : public NamespaceProvider {
    public:
      FilesystemNamespaceProvider() = default;

      const std::set<std::string> &requiredFilesystems() const;

      // NamespaceProvider interface
      bool prepare(Pool &pool) override;
      bool isSatisfied( detail::IdType value) const override;

    private:
      /** filesystems mentioned in /etc/sysconfig/storage, mutable for lazy init */
      mutable std::optional<std::set<std::string> > _requiredFilesystems;
  };

} // namespace zyppng::sat

#endif
