# libzypp-glib version — single source of truth.
# Consumed by:
#   - libzypp-glib.spec via:  %%include %%{_sourcedir}/libzypp-glib-version.inc
#   - CMake via:              zyppng/api/VERSION.cmake (regex-parses %%define lines)
#
# Versioning policy mirrors zypp/VERSION.cmake (Libtool-style):
#   MAJOR        Major number for this branch.
#   MINOR        Most recent interface number this library implements.
#   COMPATMINOR  Latest binary-compatible minor number.
#   PATCH        Implementation number of the current interface.
#
# Pre-1.0: API is unstable; any release may break ABI.
#
# SONAME math (applied by CMake, identical to libzypp):
#   CURRENT  = MAJOR*100 + MINOR
#   AGE      = MINOR - COMPATMINOR
#   SO_FIRST = CURRENT - AGE          -> SONAME
#   -> libzypp-glib.so.<SO_FIRST>.<AGE>.<PATCH>
#
# Update rules:
#   1) After branching: bump MAJOR, reset MINOR=0/PATCH=0/COMPATMINOR=0.
#   2) Bump only immediately before a public release.
#   3) Any source change since last update -> bump PATCH.
#   4) Interface added/removed/changed -> bump MINOR, PATCH=0.
#   5) Only additions since last release -> leave COMPATMINOR unchanged.
#   6) Any removal since last release -> COMPATMINOR=MINOR.

%define libzypp_glib_major       0
%define libzypp_glib_minor       1
%define libzypp_glib_patch       0
%define libzypp_glib_compatminor 0

Version: %{libzypp_glib_major}.%{libzypp_glib_minor}.%{libzypp_glib_patch}

# LAST RELEASED: 0.1.0 (0)
