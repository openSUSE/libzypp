#
# spec file for package libzypp-glib
#
# Copyright (c) 2025 SUSE LLC
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

# Version is maintained in libzypp-glib-version.inc — single source of truth
# for both this spec and the CMake build system.
%include %{_sourcedir}/libzypp-glib-version.inc

# SONAME math — mirrors zyppng/api/VERSION.cmake exactly.
# SO_FIRST = MAJOR*100 + COMPATMINOR          → SONAME
# AGE      = MINOR - COMPATMINOR
# VERSION_INFO = SO_FIRST.AGE.PATCH           → full .so filename
%define libzypp_glib_so_first    %{expr: %{libzypp_glib_major} * 100 + %{libzypp_glib_compatminor}}
%define libzypp_glib_age         %{expr: %{libzypp_glib_minor} - %{libzypp_glib_compatminor}}
%define libzypp_glib_version_info %{libzypp_glib_so_first}.%{libzypp_glib_age}.%{libzypp_glib_patch}

Name:           libzypp-glib
Release:        0
License:        GPL-2.0-or-later
URL:            https://github.com/openSUSE/libzypp
Summary:        GLib/GObject C API for the zyppng package management library
Group:          System/Packages
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Source:         %{name}-%{version}.tar.bz2
Source1:        libzypp-glib-version.inc

BuildRequires:  obs-service-cpio_repack
BuildRequires:  cmake >= 3.28
BuildRequires:  clang19
BuildRequires:  doxygen
BuildRequires:  lld19
BuildRequires:  ninja
BuildRequires:  pkg-config
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  gobject-introspection-devel
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(openssl)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(sigc++-2.0)
BuildRequires:  libboost_headers-devel
BuildRequires:  libboost_thread-devel
BuildRequires:  libboost_test-devel
BuildRequires:  yaml-cpp-devel
BuildRequires:  libgpgme-devel
BuildRequires:  gettext-devel
BuildRequires:  rpm-devel > 4.4
BuildRequires:  libsolv-devel-static >= 0.7.34
BuildRequires:  libsolv-tools-base >= 0.7.29
BuildRequires:  libzck-devel
BuildRequires:  libzstd-devel
BuildRequires:  libbz2-devel
BuildRequires:  xz-devel

BuildRequires:  rubygem(asciidoctor)
BuildRequires:  graphviz
BuildRequires:  graphviz-gd

#testcases
BuildRequires:  nginx
BuildRequires:  vsftpd
BuildRequires:  squid
BuildRequires:  FastCGI-devel

%requires_eq    libsolv-tools-base

%description
libzypp-glib is the GLib/GObject C API for zyppng, the next-generation
package management library. It exposes a GObject-Introspectable interface
suitable for consumption from Python, JavaScript, Vala and other languages
via GObject Introspection.

#-----------------------------------------------------------------------
%package devel
Summary:        Development files for libzypp-glib
Group:          Development/Libraries/C and C++
Requires:       libzypp-glib = %{version}
Requires:       pkgconfig(glib-2.0)
Requires:       pkgconfig(gobject-2.0)
Requires:       pkgconfig(gio-2.0)

%description devel
Development headers and pkg-config file for libzypp-glib.

#-----------------------------------------------------------------------
%package -n typelib-1_0-Zypp-1_0
Summary:        GObject Introspection typelib for libzypp-glib
Group:          System/Libraries
Requires:       libzypp-glib = %{version}

%description -n typelib-1_0-Zypp-1_0
GObject Introspection typelib for libzypp-glib, enabling consumption
from Python, JavaScript, Vala and other GI-capable languages.

#-----------------------------------------------------------------------
%prep
%autosetup -p1

%build
mkdir build
cd build

export CC=clang-19
export CXX=clang++-19

cmake .. \
      -G Ninja \
      -DCMAKE_INSTALL_PREFIX=%{_prefix} \
      -DLIB=%{_lib} \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SKIP_RPATH=1 \
      -DCMAKE_INSTALL_LIBEXECDIR=%{_libexecdir} \
      -DZYPPCONFDIR=%{_prefix}/etc \
      -DBUILD_LIBZYPP=OFF \
      -DBUILD_LIBZYPPNG=ON \
      -DENABLE_ZSTD_COMPRESSION=1 \
      -DENABLE_ZCHUNK_COMPRESSION=1

ninja %{?_smp_mflags}

%install
cd build
DESTDIR=%{buildroot} ninja install

%check
cd build
ctest --output-on-failure %{?_smp_mflags}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%license COPYING
%{_libdir}/libzypp-glib.so.%{libzypp_glib_version_info}
%{_libdir}/libzypp-glib.so.%{libzypp_glib_so_first}
%dir %{_libexecdir}/zyppng
%dir %{_libexecdir}/zyppng/workers
%{_libexecdir}/zyppng/workers/zypp-media-chksum
%{_libexecdir}/zyppng/workers/zypp-media-copy
%{_libexecdir}/zyppng/workers/zypp-media-dir
%{_libexecdir}/zyppng/workers/zypp-media-disc
%{_libexecdir}/zyppng/workers/zypp-media-disk
%{_libexecdir}/zyppng/workers/zypp-media-http
%{_libexecdir}/zyppng/workers/zypp-media-iso
%{_libexecdir}/zyppng/workers/zypp-media-nfs
%{_libexecdir}/zyppng/workers/zypp-media-smb

%files devel
%defattr(-,root,root)
%{_libdir}/libzypp-glib.so
%{_includedir}/zypp-glib/
%{_datadir}/gir-1.0/Zypp-1.0.gir

%files -n typelib-1_0-Zypp-1_0
%defattr(-,root,root)
%{_libdir}/girepository-1.0/Zypp-1.0.typelib

%changelog
