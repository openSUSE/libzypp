#
# spec file for package libzypp
#
# Copyright (c) 2023 SUSE LLC
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://bugs.opensuse.org/
#

# Switched to single_rpmtrans as default install backed.
# SUSE distros stay with classic_rpmtrans as default.
# Code16: Want's to switch to single_rpmtrans as default
#         (level of enablement is handled in the code)
%if 0%{?suse_version} && 0%{?suse_version} < 1600
%bcond_without classic_rpmtrans_as_default
%else
%bcond_with classic_rpmtrans_as_default
%endif

# In Code16 libsolv moved the static libs from -devel to -devel-static.
# Those are needed while cmake -DSUSE enforces linking libsolv statically.
%if 0%{?suse_version} >= 1600
%define libsolv_devel_package libsolv-devel-static
%else
%define libsolv_devel_package libsolv-devel
%endif

%if 0%{?suse_version} > 1500 || 0%{?sle_version} >= 150400 || (0%{?is_opensuse} && 0%{?sle_version} >= 150100)
%bcond_without zchunk
%else
%bcond_with zchunk
%endif

# libsolvs external references might require us to link against zstd, bz2, xz
%if 0%{?sle_version} >= 150000 || 0%{?suse_version} >= 1500
%bcond_without zstd
%else
%bcond_with zstd
%endif
%if 0%{?sle_version} >= 120300 || 0%{?suse_version} >= 1330 || !0%{?suse_version}
%bcond_without bz2
%bcond_without xz
%else
%bcond_with bz2
%bcond_with xz
%endif

%bcond_without mediabackend_tests

# older libsigc versions have a bug that causes a segfault
# when clearing connections during signal emission
# see https://bugzilla.gnome.org/show_bug.cgi?id=784550
%if 0%{?sle_version} < 150200
%bcond_without sigc_block_workaround
%else
%bcond_with sigc_block_workaround
%endif


%if 0%{?suse_version} > 1500 || 0%{?sle_version} >= 150600
%bcond_without visibility_hidden
%else
%bcond_with visibility_hidden
%endif

# We're going successively to move our default configuration into /usr/etc/.
# The final configuration data will then be merged according to the rules
# defined by the UAPI.6 Configuration Files Specification from:
#   system-wide configuration (/etc    /zypp/zypp.conf[.d/*.conf])
#   vendor configuration      (/usr/etc/zypp/zypp.conf[.d/*.conf])
# Note: Kind of related to '_distconfdir', but 'zyppconfdir' refers to
# where we install and lookup our own config files. While '_distconfdir'
# tells where we install other packages config snippets (e.g. logrotate).
%if %{defined _distconfdir}
 %define zyppconfdir %{_distconfdir}
%else
 %if 0%{?fedora}
  # https://github.com/openSUSE/libzypp/issues/693
  %define zyppconfdir %{_prefix}/share
 %else
  %define zyppconfdir %{_prefix}/etc
 %endif
%endif

%if 0%{?suse_version} && 0%{?suse_version} < 1610
# legacy distro tools may not be prepared for having no /etc/zypp.conf
%bcond_without keep_legacy_zyppconf
%else
%bcond_with keep_legacy_zyppconf
%endif

Name:           libzypp
Version:        @VERSION@
Release:        0
License:        GPL-2.0-or-later
URL:            https://github.com/openSUSE/libzypp
Summary:        Library for package, patch, pattern and product management
Group:          System/Packages
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Source:         %{name}-%{version}.tar.bz2
Source1:        %{name}-rpmlintrc
Provides:       yast2-packagemanager
Obsoletes:      yast2-packagemanager

# bsc#1227793:  python zypp-plugin < 0.6.4 rejects stomp headers including a '-'
Conflicts:      python2-zypp-plugin < 0.6.4
Conflicts:      python3-zypp-plugin < 0.6.4
# API refactoring. Prevent zypper from using (now) private symbols
Conflicts:      zypper < 1.14.91

# Features we provide (update doc/autoinclude/FeatureTest.doc):
Provides:       libzypp(plugin) = 0.1
Provides:       libzypp(plugin:appdata) = 0
Provides:       libzypp(plugin:commit) = 1
Provides:       libzypp(plugin:services) = 1
Provides:       libzypp(plugin:system) = 1
Provides:       libzypp(plugin:urlresolver) = 0
Provides:       libzypp(plugin:repoverification) = 0
Provides:       libzypp(repovarexpand) = 1.1
Provides:       libzypp(econf) = 0

%if 0%{?suse_version}
Recommends:     logrotate
# lsof is used for 'zypper ps':
Recommends:     lsof
%endif
BuildRequires:  cmake >= 3.17
BuildRequires:  pkgconfig(openssl)
BuildRequires:  pkgconfig(libudev)
%if 0%{?suse_version} >= 1330
BuildRequires:  libboost_headers-devel
BuildRequires:  libboost_program_options-devel
BuildRequires:  libboost_test-devel
BuildRequires:  libboost_thread-devel
%else
BuildRequires:  boost-devel
%endif
BuildRequires:  dejagnu
BuildRequires:  doxygen
BuildRequires:  texlive-latex
BuildRequires:  texlive-xcolor
BuildRequires:  texlive-newunicodechar
BuildRequires:  texlive-dvips
BuildRequires:  ghostscript
BuildRequires:  gcc-c++ >= 7
BuildRequires:  gettext-devel
BuildRequires:  graphviz
BuildRequires:  libxml2-devel
BuildRequires:  yaml-cpp-devel

# we are loading libproxy dynamically, however we have
# a failsafe unit test that links against libproxy to make
# sure the API did not change
BuildRequires:  libproxy-devel

#keep the libproxy runtime requires for old releases
%if 0%{?suse_version} && 0%{?suse_version} <= 1500 && 0%{?sle_version} <= 150500
Requires: libproxy1
%endif

%if 0%{?fedora_version} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires:  pkgconfig
%else
BuildRequires:  pkg-config
%endif

BuildRequires:  %{libsolv_devel_package} >= 0.7.34
%if 0%{?suse_version} > 1500 || 0%{?sle_version} >= 150600
BuildRequires:  libsolv-tools-base >= 0.7.29
%requires_eq    libsolv-tools-base
%else
BuildRequires:  libsolv-tools
%requires_eq    libsolv-tools
%endif

BuildRequires:  glib2-devel
BuildRequires:  libsigc++2-devel
BuildRequires:  readline-devel >= 5.1

# required for testsuite
%if %{with mediabackend_tests}
BuildRequires:  nginx
BuildRequires:	vsftpd
BuildRequires:	squid
%endif

Requires:       rpm
BuildRequires:  rpm

%if 0%{?suse_version}
BuildRequires:  rpm-devel > 4.4
%endif

%if 0%{?fedora_version} || 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600
BuildRequires:  popt-devel
BuildRequires:  rpm-devel > 4.4
%endif

%if 0%{?mandriva_version}
BuildRequires:  librpm-devel > 4.4
%endif

%if 0%{?suse_version}
BuildRequires:  libgpgme-devel
#testsuite
%if %{with mediabackend_tests}
BuildRequires:  FastCGI-devel
%endif
%else
BuildRequires:  gpgme-devel
#testsuite
%if %{with mediabackend_tests}
BuildRequires:	fcgi-devel
%endif
%endif

%define min_curl_version 7.19.4
BuildRequires:  libcurl-devel >= %{min_curl_version}
BuildRequires:  ( libcurl4 without libcurl-mini4 )
%if 0%{?suse_version}
# Code11+
Requires:       libcurl4   >= %{min_curl_version}
%else
# Other distros (Fedora)
Requires:       libcurl   >= %{min_curl_version}
%endif

# required for documentation
%if 0%{?suse_version} >= 1330
BuildRequires:  rubygem(asciidoctor)
%else
BuildRequires:  asciidoc
BuildRequires:  libxslt-tools
%endif

%if %{with zchunk}
BuildRequires:  libzck-devel
%endif

%if %{with zstd}
BuildRequires:  libzstd-devel
%endif

%if %{with bz2}
%if 0%{?suse_version}
BuildRequires:  libbz2-devel
%else
BuildRequires:  bzip2-devel
%endif
%endif

%if %{with xz}
BuildRequires:  xz-devel
%endif

%description
libzypp is the package management library that powers applications
like YaST, zypper and the openSUSE/SLE implementation of PackageKit.

libzypp provides functionality for a package manager:

  * An API for package repository management, supporting most common
    repository metadata formats and signed repositories.
  * An API for solving packages, products, patterns and patches
    (installation, removal, update and distribution upgrade
    operations) dependencies, with additional features like locking.
  * An API for commiting the transaction to the system over a rpm
    target. Supporting deltarpm calculation, media changing and
    installation order calculation.
  * An API for browsing available and installed software, with some
    facilities for programs with an user interface.

%package devel
Summary:        Header files for libzypp, a library for package management
Group:          Development/Libraries/C and C++
Provides:       yast2-packagemanager-devel
Obsoletes:      yast2-packagemanager-devel
Provides:       libzypp-tui-devel = 1
%if 0%{?suse_version} >= 1330
Requires:       libboost_headers-devel
Requires:       libboost_program_options-devel
Requires:       libboost_test-devel
Requires:       libboost_thread-devel
%else
Requires:       boost-devel
%endif
Requires:       bzip2
Requires:       glibc-devel
Requires:       libstdc++-devel
Requires:       libxml2-devel
Requires:       libzypp = %{version}
Requires:       pkgconfig(openssl)
Requires:       popt-devel
Requires:       rpm-devel > 4.4
Requires:       zlib-devel
Requires:       libudev-devel
%if 0%{?suse_version}
# Code11+
Requires:       libcurl-devel >= %{min_curl_version}
%else
# Other distros (Fedora)
Requires:       libcurl-devel >= %{min_curl_version}
%endif
%if 0%{?suse_version}
%requires_ge    %{libsolv_devel_package}
%else
Requires:       %{libsolv_devel_package}
%endif

%description devel
Development files for libzypp, a library for package, patch, pattern
and product management.

%package devel-doc
Summary:        Developer documentation for libzypp
Group:          Documentation/HTML

%description devel-doc
Developer documentation for libzypp.

%prep
%autosetup -p1

%build
mkdir build
cd build

export CFLAGS="%{optflags}"
export CXXFLAGS="%{optflags}"

CMAKE_FLAGS=
%if 0%{?fedora} || 0%{?rhel} >= 6
CMAKE_FLAGS="-DFEDORA=1"
%endif
%if 0%{?mageia}
CMAKE_FLAGS="-DMAGEIA=1"
%endif
%if 0%{?suse_version}
CMAKE_FLAGS="-DSUSE=1"
%endif

EXTRA_CMAKE_OPTIONS=
%if 0%{?suse_version}
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DLIBZYPP_CODESTREAM=0%{?suse_version}:0%{?sle_version}:0%{?is_opensuse}"

%if 0%{?suse_version} <= 1500 && 0%{?sle_version} <= 150600
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DLIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT=1"
%endif

%if 0%{?suse_version} < 1600
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DLIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT=1"
EXTRA_CMAKE_OPTIONS="${EXTRA_CMAKE_OPTIONS} -DLIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT=1"
%endif

%endif

cmake .. $CMAKE_FLAGS \
      -DCMAKE_INSTALL_PREFIX=%{_prefix} \
      -DDOC_INSTALL_DIR=%{_docdir} \
      -DLIB=%{_lib} \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SKIP_RPATH=1 \
      -DCMAKE_INSTALL_LIBEXECDIR=%{_libexecdir} \
      -DZYPPCONFDIR=%{zyppconfdir} \
      %{?with_keep_legacy_zyppconf:-DKEEP_LEGACY_ZYPPCONF=1} \
      %{?with_visibility_hidden:-DENABLE_VISIBILITY_HIDDEN=1} \
      %{?with_zchunk:-DENABLE_ZCHUNK_COMPRESSION=1} \
      %{?with_zstd:-DENABLE_ZSTD_COMPRESSION=1} \
      %{?with_sigc_block_workaround:-DENABLE_SIGC_BLOCK_WORKAROUND=1} \
      %{!?with_mediabackend_tests:-DDISABLE_MEDIABACKEND_TESTS=1} \
      %{?with_classic_rpmtrans_as_default:-DLIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT=1} \
      ${EXTRA_CMAKE_OPTIONS}

make %{?_smp_mflags} VERBOSE=1

%install
cd build
%make_install
%if 0%{?fedora_version} || 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600
ln -s %{_sysconfdir}/yum.repos.d %{buildroot}/%{_sysconfdir}/zypp/repos.d
%else
mkdir -p %{buildroot}/%{_sysconfdir}/zypp/repos.d
%endif
# system wide config only:
mkdir -p %{buildroot}/%{_sysconfdir}/zypp/services.d
mkdir -p %{buildroot}/%{_sysconfdir}/zypp/systemCheck.d
mkdir -p %{buildroot}/%{_sysconfdir}/zypp/vars.d
mkdir -p %{buildroot}/%{_sysconfdir}/zypp/vendors.d
mkdir -p %{buildroot}/%{_sysconfdir}/zypp/multiversion.d
mkdir -p %{buildroot}/%{_sysconfdir}/zypp/needreboot.d
mkdir -p %{buildroot}/%{_sysconfdir}/zypp/credentials.d
# system and vendor config supported:
mkdir -p %{buildroot}/%{_sysconfdir}/zypp/zypp.conf.d
mkdir -p %{buildroot}/%{zyppconfdir}/zypp/zypp.conf.d
#
mkdir -p %{buildroot}/%{_prefix}/lib/zypp
mkdir -p %{buildroot}/%{_prefix}/lib/zypp/plugins
mkdir -p %{buildroot}/%{_prefix}/lib/zypp/plugins/appdata
mkdir -p %{buildroot}/%{_prefix}/lib/zypp/plugins/commit
mkdir -p %{buildroot}/%{_prefix}/lib/zypp/plugins/services
mkdir -p %{buildroot}/%{_prefix}/lib/zypp/plugins/system
mkdir -p %{buildroot}/%{_prefix}/lib/zypp/plugins/urlresolver
mkdir -p %{buildroot}/%{_var}/lib/zypp
mkdir -p %{buildroot}/%{_var}/log/zypp
mkdir -p %{buildroot}/%{_var}/cache/zypp

cd ..

# Create filelist with translations
%{find_lang} zypp

%if %{defined _distconfdir}
# Move logratate files form /etc/logrotate.d to /usr/etc/logrotate.d
mkdir -p %{buildroot}/%{_distconfdir}/logrotate.d
mv %{buildroot}/%{_sysconfdir}/logrotate.d/zypp-history.lr %{buildroot}%{_distconfdir}/logrotate.d
%endif

%check
pushd build
LD_LIBRARY_PATH="$(pwd)/../zypp:$LD_LIBRARY_PATH" ctest --output-on-failure .
popd

# Some tool macros for the scripts:
# Prepare for migration to /usr/etc; save any old .rpmsave in pre and restore new one's in posttrans:
%define rpmsaveSave()    test -f %{1}.rpmsave && mv -v %{1}.rpmsave %{1}.rpmsave.old ||:
%define rpmsaveRestore() test -f %{1}.rpmsave && mv -v %{1}.rpmsave %{1} ||:

%pre
%if %{defined _distconfdir}
%rpmsaveSave %{_sysconfdir}/logrotate.d/zypp-history.lr
%endif
%if %{without keep_legacy_zyppconf}
%rpmsaveSave %{_sysconfdir}/zypp/zypp.conf
%endif

%posttrans
%if %{defined _distconfdir}
%rpmsaveRestore %{_sysconfdir}/logrotate.d/zypp-history.lr
%endif
%if %{without keep_legacy_zyppconf}
%rpmsaveRestore %{_sysconfdir}/zypp/zypp.conf
%endif

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files -f zypp.lang
%defattr(-,root,root)
%if 0%{?suse_version} >= 1500
%license COPYING
%endif
%dir               %{_sysconfdir}/zypp
%if 0%{?fedora_version} || 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600
%{_sysconfdir}/zypp/repos.d
%else
%dir               %{_sysconfdir}/zypp/repos.d
%endif
%dir               %{_sysconfdir}/zypp/services.d
%dir               %{_sysconfdir}/zypp/systemCheck.d
%dir               %{_sysconfdir}/zypp/vars.d
%dir               %{_sysconfdir}/zypp/vendors.d
%dir               %{_sysconfdir}/zypp/multiversion.d
%config(noreplace) %{_sysconfdir}/zypp/needreboot
%dir               %{_sysconfdir}/zypp/needreboot.d
%dir               %{_sysconfdir}/zypp/credentials.d
%config(noreplace) %{_sysconfdir}/zypp/systemCheck
# system and vendor config supported:
%if %{undefined _distconfdir}
%dir %{zyppconfdir}
%endif
%dir %{zyppconfdir}/zypp
#
%{zyppconfdir}/zypp/zypp.conf
%if %{with keep_legacy_zyppconf}
%config(noreplace) %{_sysconfdir}/zypp/zypp.conf
%else
%{_sysconfdir}/zypp/zypp.conf.README
%endif
%dir %{zyppconfdir}/zypp/zypp.conf.d
%dir %{_sysconfdir}/zypp/zypp.conf.d
#
%if %{defined _distconfdir}
%{_distconfdir}/logrotate.d/zypp-history.lr
%else
%config(noreplace) %{_sysconfdir}/logrotate.d/zypp-history.lr
%endif
%dir               %{_var}/lib/zypp
%if "%{_libexecdir}" != "%{_prefix}/lib"
%dir               %{_libexecdir}/zypp
%endif
%dir %attr(750,root,root) %{_var}/log/zypp
%dir               %{_var}/cache/zypp
%{_prefix}/lib/zypp
%{_datadir}/zypp
%{_bindir}/*
%{_libexecdir}/zypp/zypp-rpm
%{_libdir}/libzypp*so.*
%doc %{_mandir}/man1/*.1.*
%doc %{_mandir}/man5/*.5.*

%files devel
%defattr(-,root,root)
%{_libdir}/cmake/Zypp
%{_libdir}/libzypp.so
%{_libdir}/libzypp-tui.a
%{_includedir}/zypp
%{_includedir}/zypp-core
%{_includedir}/zypp-media
%{_includedir}/zypp-curl
%{_includedir}/zypp-tui
%{_includedir}/zypp-common
%{_libdir}/pkgconfig/libzypp.pc

%files devel-doc
%defattr(-,root,root)
%{_docdir}/%{name}

%changelog
