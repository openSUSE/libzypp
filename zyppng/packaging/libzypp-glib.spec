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

%define sover %{libzypp_glib_major}

Name:           libzypp-glib
Release:        0
License:        GPL-2.0-or-later
URL:            https://github.com/openSUSE/libzypp
Summary:        GLib/GObject C API for the zyppng package management library
Group:          System/Packages
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Source:         %{name}-%{version}.tar.bz2
Source1:        libzypp-glib-version.inc

# libzypp-glib is the NG-only standalone library; it does NOT link libzypp.so.
# It bundles zyppng as a static archive and exposes a GObject-Introspectable C API.
# Requires Clang 19 — GCC is not supported for this target.

BuildRequires:  cmake >= 3.28
BuildRequires:  clang19
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
BuildRequires:  yaml-cpp-devel
BuildRequires:  libgpgme-devel
BuildRequires:  gettext-devel
BuildRequires:  rpm-devel > 4.4
BuildRequires:  libsolv-devel-static >= 0.7.34
BuildRequires:  libsolv-tools-base >= 0.7.29
BuildRequires:  libzstd-devel
BuildRequires:  libbz2-devel
BuildRequires:  xz-devel

%description
libzypp-glib is the GLib/GObject C API for zyppng, the next-generation
package management library. It exposes a GObject-Introspectable interface
suitable for consumption from Python, JavaScript, Vala and other languages
via GObject Introspection.

This package is independent of the legacy libzypp.so and bundles the zyppng
C++20 module as a static archive.

#-----------------------------------------------------------------------
%package -n libzypp-glib%{sover}
Summary:        GLib/GObject C API for the zyppng package management library
Group:          System/Libraries
%requires_eq    libsolv-tools-base

%description -n libzypp-glib%{sover}
Runtime library for libzypp-glib.

#-----------------------------------------------------------------------
%package devel
Summary:        Development files for libzypp-glib
Group:          Development/Libraries/C and C++
Requires:       libzypp-glib%{sover} = %{version}
Requires:       pkgconfig(glib-2.0)
Requires:       pkgconfig(gobject-2.0)
Requires:       pkgconfig(gio-2.0)

%description devel
Development headers and pkg-config file for libzypp-glib.

#-----------------------------------------------------------------------
%package -n typelib-1_0-Zypp-1_0
Summary:        GObject Introspection typelib for libzypp-glib
Group:          System/Libraries
Requires:       libzypp-glib%{sover} = %{version}

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

%post -n libzypp-glib%{sover} -p /sbin/ldconfig
%postun -n libzypp-glib%{sover} -p /sbin/ldconfig

%files -n libzypp-glib%{sover}
%defattr(-,root,root)
%license COPYING
%{_libdir}/libzypp-glib.so.%{version}
%{_libdir}/libzypp-glib.so.%{sover}
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
