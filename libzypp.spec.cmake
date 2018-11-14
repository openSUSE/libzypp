#
# spec file for package libzypp
#
# Copyright (c) 2005-2013 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#


Name:           @PACKAGE@
Version:        @VERSION@
Release:        0
License:        GPL-2.0+
Url:            https://github.com/openSUSE/libzypp
Summary:        Library for package, patch, pattern and product management
Group:          System/Packages
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Source:         %{name}-%{version}.tar.bz2
Source1:        %{name}-rpmlintrc
Provides:       yast2-packagemanager
Obsoletes:      yast2-packagemanager

# Features we provide (update doc/autoinclude/FeatureTest.doc):
Provides:       libzypp(plugin) = 0.1
Provides:       libzypp(plugin:appdata) = 0
Provides:       libzypp(plugin:commit) = 1
Provides:       libzypp(plugin:services) = 1
Provides:       libzypp(plugin:system) = 1
Provides:       libzypp(plugin:urlresolver) = 0
Provides:       libzypp(repovarexpand) = 1.1

%if 0%{?suse_version}
Recommends:     logrotate
# lsof is used for 'zypper ps':
Recommends:     lsof
%endif
BuildRequires:  cmake
BuildRequires:  openssl-devel
%if 0%{?suse_version} >= 1130 || 0%{?fedora_version} >= 16
BuildRequires:  pkgconfig(libudev)
%else
BuildRequires:  hal-devel
%endif
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
BuildRequires:  gcc-c++ >= 4.6
BuildRequires:  gettext-devel
BuildRequires:  graphviz
BuildRequires:  libxml2-devel
%if 0%{?suse_version} != 1110
# No libproxy on SLES
BuildRequires:  libproxy-devel
%endif

%if 0%{?fedora_version} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires:  pkgconfig
%else
BuildRequires:  pkg-config
%endif

BuildRequires:  libsolv-devel >= 0.7.1
%if 0%{?suse_version} >= 1100
BuildRequires:  libsolv-tools
%requires_eq    libsolv-tools
%else
Requires:       libsolv-tools
%endif

# required for testsuite, webrick
BuildRequires:  ruby

Requires:       rpm

%if 0%{?suse_version}
BuildRequires:  rpm-devel > 4.4
%endif

%if 0%{?fedora_version} || 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600
BuildRequires:  glib2-devel
BuildRequires:  popt-devel
BuildRequires:  rpm-devel > 4.4
%endif

%if 0%{?mandriva_version}
BuildRequires:  glib2-devel
BuildRequires:  librpm-devel > 4.4
%endif

%if 0%{?suse_version}
BuildRequires:  libgpgme-devel
%else
BuildRequires:  gpgme-devel
%endif

%define min_curl_version 7.19.4
%if 0%{?suse_version}
%if 0%{?suse_version} >= 1100
# Code11+
BuildRequires:  libcurl-devel >= %{min_curl_version}
Requires:       libcurl4   >= %{min_curl_version}
%else
# Code10
BuildRequires:  curl-devel
%endif
%else
# Other distros (Fedora)
BuildRequires:  libcurl-devel >= %{min_curl_version}
Requires:       libcurl   >= %{min_curl_version}
%endif

# required for documentation
%if 0%{?suse_version} >= 1330
BuildRequires:  rubygem(asciidoctor)
%else
BuildRequires:  asciidoc
BuildRequires:  libxslt-tools
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
Requires:       openssl-devel
Requires:       popt-devel
Requires:       rpm-devel > 4.4
Requires:       zlib-devel
%if 0%{?suse_version} >= 1130 || 0%{?fedora_version} >= 16
Requires:       libudev-devel
%else
Requires:       hal-devel
%endif
%if 0%{?suse_version}
%if 0%{?suse_version} >= 1100
# Code11+
Requires:       libcurl-devel >= %{min_curl_version}
%else
# Code10
Requires:       curl-devel
%endif
%else
# Other distros (Fedora)
Requires:       libcurl-devel >= %{min_curl_version}
%endif
%if 0%{?suse_version} >= 1100
%requires_ge    libsolv-devel
%else
Requires:       libsolv-devel
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
%setup -q

%build
mkdir build
cd build
export CFLAGS="$RPM_OPT_FLAGS"
export CXXFLAGS="$RPM_OPT_FLAGS"
unset EXTRA_CMAKE_OPTIONS
# No libproxy on SLE11
%if 0%{?suse_version} == 1110
export EXTRA_CMAKE_OPTIONS="-DDISABLE_LIBPROXY=ON"
%endif

cmake -DCMAKE_INSTALL_PREFIX=%{_prefix} \
      -DDOC_INSTALL_DIR=%{_docdir} \
      -DLIB=%{_lib} \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SKIP_RPATH=1 \
      ${EXTRA_CMAKE_OPTIONS} \
      ..
make %{?_smp_mflags} VERBOSE=1
make -C doc/autodoc %{?_smp_mflags}
make -C po %{?_smp_mflags} translations
make -C tests %{?_smp_mflags}

%install
rm -rf "$RPM_BUILD_ROOT"
cd build
make install DESTDIR=$RPM_BUILD_ROOT
make -C doc/autodoc install DESTDIR=$RPM_BUILD_ROOT
%if 0%{?fedora_version} || 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600
ln -s %{_sysconfdir}/yum.repos.d $RPM_BUILD_ROOT%{_sysconfdir}/zypp/repos.d
%else
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/zypp/repos.d
%endif
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/zypp/services.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/zypp/systemCheck.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/zypp/vars.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/zypp/vendors.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/zypp/multiversion.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/zypp/needreboot.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/zypp/credentials.d
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib/zypp
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib/zypp/plugins
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib/zypp/plugins/appdata
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib/zypp/plugins/commit
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib/zypp/plugins/services
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib/zypp/plugins/system
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib/zypp/plugins/urlresolver
mkdir -p $RPM_BUILD_ROOT%{_var}/lib/zypp
mkdir -p $RPM_BUILD_ROOT%{_var}/log/zypp
mkdir -p $RPM_BUILD_ROOT%{_var}/cache/zypp

# Default to 'solver.dupAllowVendorChange = false' on TW and post SLE12
%if 0%{?suse_version} >= 1330 || "%{distribution}" == "openSUSE Tumbleweed"
sed -i "s|# solver.dupAllowVendorChange = true|solver.dupAllowVendorChange = false|g" %{buildroot}%{_sysconfdir}/zypp/zypp.conf
%endif

make -C po install DESTDIR=$RPM_BUILD_ROOT
# Create filelist with translations
cd ..
%{find_lang} zypp

%check
pushd build/tests
LD_LIBRARY_PATH=$RPM_BUILD_ROOT%{_libdir}:${LD_LIBRARY_PATH} ctest .
popd

%post
/sbin/ldconfig
if [ -f /var/cache/zypp/zypp.db ]; then rm /var/cache/zypp/zypp.db; fi

# convert old lock file to new
# TODO make this a separate file?
# TODO run the sript only when updating form pre-11.0 libzypp versions
LOCKSFILE=%{_sysconfdir}/zypp/locks
OLDLOCKSFILE=%{_sysconfdir}/zypp/locks.old

is_old(){
  # if no such file, exit with false (1 in bash)
  test -f ${LOCKSFILE} || return 1
  TEMP_FILE=`mktemp`
  cat ${LOCKSFILE} | sed '/^\#.*/ d;/.*:.*/d;/^[^[a-zA-Z\*?.0-9]*$/d' > ${TEMP_FILE}
  if [ -s ${TEMP_FILE} ]
  then
    RES=0
  else
    RES=1
  fi
  rm -f ${TEMP_FILE}
  return ${RES}
}

append_new_lock(){
  case "$#" in
    1 )
  echo "
solvable_name: $1
match_type: glob
" >> ${LOCKSFILE}
;;
    2 ) #TODO version
  echo "
solvable_name: $1
match_type: glob
version: $2
" >> ${LOCKSFILE}
;;
    3 ) #TODO version
  echo "
solvable_name: $1
match_type: glob
version: $2 $3
" >> ${LOCKSFILE}
  ;;
esac
}

die() {
  echo $1
  exit 1
}

if is_old ${LOCKSFILE}
  then
  mv -f ${LOCKSFILE} ${OLDLOCKSFILE} || die "cannot backup old locks"
  cat ${OLDLOCKSFILE}| sed "/^\#.*/d"| while read line
  do
    append_new_lock $line
  done
fi

%postun -p /sbin/ldconfig

%clean
rm -rf "$RPM_BUILD_ROOT"

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
%config(noreplace) %{_sysconfdir}/zypp/zypp.conf
%config(noreplace) %{_sysconfdir}/zypp/systemCheck
%config(noreplace) %{_sysconfdir}/logrotate.d/zypp-history.lr
%dir               %{_var}/lib/zypp
%dir               %{_var}/log/zypp
%dir               %{_var}/cache/zypp
%{_prefix}/lib/zypp
%{_datadir}/zypp
%{_bindir}/*
%{_libdir}/libzypp*so.*
%doc %{_mandir}/man1/*.1.*
%doc %{_mandir}/man5/*.5.*

%files devel
%defattr(-,root,root)
%{_libdir}/libzypp.so
%{_includedir}/zypp
%{_datadir}/cmake/Modules/*
%{_libdir}/pkgconfig/libzypp.pc

%files devel-doc
%defattr(-,root,root)
%{_docdir}/%{name}

%changelog
