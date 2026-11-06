# libzypp

libzypp is the package management library of openSUSE and SUSE Linux
Enterprise. It manages repositories and services, downloads and verifies
metadata and packages, resolves dependencies (using
[libsolv](https://github.com/openSUSE/libsolv)) and installs or removes rpm
packages.

It is used by [zypper](https://github.com/openSUSE/zypper), YaST, PackageKit,
Agama and other tools. Its API and ABI are therefore a contract with many
consumers. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before changing
anything.

## Repository layout

| Directory | Content |
|---|---|
| `zypp/` | The libzypp library (`libzypp.so`): sources in `zypp/zypp`, plus `zypp-tui`, `zypp-compat` (compatibility headers), tools, tests, documentation and packaging (`zypp/package/`, `zypp/libzypp.spec.cmake`) |
| `zypp-logic/` | Code compiled into libzypp and shared with the next-generation code: many core classes (`Url`, `Pathname`, `Arch`, `Edition`, …), the curl and media helpers, and the async/network framework in `*/ng/` |
| `zyppng/` | The next-generation (NG) rewrite in C++20 modules and the experimental `libzypp-glib` API. **Development is paused**; see [zyppng/CONTRIBUTING.md](zyppng/CONTRIBUTING.md) |
| `maintainer-docs/` | Maintainer handbook: architecture, concepts, pitfalls, triage and release procedures. Start with [maintainer-docs/README.md](maintainer-docs/README.md) |
| `po/` | Translations (maintained on [openSUSE Weblate](https://l10n.opensuse.org/projects/libzypp/)) |
| `cmake/`, `external/` | CMake modules and vendored header-only polyfills |

## Building

The build requirements are listed in `zypp/libzypp.spec.cmake` (libsolv,
librpm, libcurl, gpgme, libxml2, yaml-cpp, boost, …).

```sh
mkdir build && cd build
cmake -DENABLE_BUILD_TESTS=ON -DDISABLE_AUTODOCS=ON ..
make -j$(nproc)
ctest --output-on-failure
```

Useful CMake options (see the top-level `CMakeLists.txt` for all of them):

* `ENABLE_BUILD_TESTS`, `ENABLE_BUILD_DOCS`, `ENABLE_BUILD_TRANS`: build the
  test suite, the doxygen documentation and the translations.
* `DISABLE_MEDIABACKEND_TESTS`: skip the tests that need nginx, vsftpd, squid
  and libfcgi.
* `ENABLE_DEVEL_BUILD`: use helper binaries such as `zypp-rpm` from the build
  directory instead of the installed ones.

To work on libsolv, libzypp and zypper together, use
[zypp-stack](https://github.com/openSUSE/zypp-stack), which builds the whole
stack in one CMake project.

## Documentation

* API documentation: <https://opensuse.github.io/libzypp/>
* Man pages (`zypp.conf(5)`, `locks(5)`, …): `zypp/doc/`
* Maintainer handbook: [maintainer-docs/](maintainer-docs/README.md)

## Reporting bugs

* openSUSE: <https://bugzilla.opensuse.org>, SUSE Linux Enterprise:
  <https://bugzilla.suse.com> or your SUSE support contact. Use the component
  `libzypp`.
* GitHub issues and pull requests are welcome too.
* Please attach `/var/log/zypper.log` and, for dependency problems, a solver
  testcase (`zypper --debug-solver <command>`). These files can contain repository
  URLs with access tokens: check them before attaching them publicly.
* **Security issues must not be reported publicly.** See
  [CONTRIBUTING.md](CONTRIBUTING.md#security).

## License

libzypp is licensed under the GNU General Public License, version 2 or later
(GPL-2.0-or-later). See [COPYING](COPYING).
