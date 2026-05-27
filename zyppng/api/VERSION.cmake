# Parse libzypp-glib version from the packaging source of truth.
# See: zyppng/packaging/libzypp-glib-version.inc

set(_LIBZYPPGLIB_VERSION_INC "${CMAKE_CURRENT_LIST_DIR}/../packaging/libzypp-glib-version.inc")

if(NOT EXISTS "${_LIBZYPPGLIB_VERSION_INC}")
  message(FATAL_ERROR "libzypp-glib version include missing: ${_LIBZYPPGLIB_VERSION_INC}")
endif()

# Extract each %define line. The .inc is the single source of truth; we never write it.
foreach(_field MAJOR MINOR PATCH COMPATMINOR)
  string(TOLOWER "${_field}" _field_lc)
  file(STRINGS "${_LIBZYPPGLIB_VERSION_INC}" _line
       REGEX "^%define[ \t]+libzypp_glib_${_field_lc}[ \t]+[0-9]+")
  if(NOT _line)
    message(FATAL_ERROR "libzypp-glib-version.inc: missing %define libzypp_glib_${_field_lc}")
  endif()
  string(REGEX REPLACE "^%define[ \t]+libzypp_glib_${_field_lc}[ \t]+([0-9]+).*" "\\1"
         _value "${_line}")
  set(LIBZYPPGLIB_${_field} "${_value}")
endforeach()

# Libtool-style SONAME math — identical to zypp/CMakeLists.txt:55-56.
math(EXPR LIBZYPPGLIB_CURRENT  "${LIBZYPPGLIB_MAJOR} * 100 + ${LIBZYPPGLIB_MINOR}")
math(EXPR LIBZYPPGLIB_AGE      "${LIBZYPPGLIB_MINOR} - ${LIBZYPPGLIB_COMPATMINOR}")
math(EXPR LIBZYPPGLIB_SO_FIRST "${LIBZYPPGLIB_CURRENT} - ${LIBZYPPGLIB_AGE}")
math(EXPR LIBZYPPGLIB_NUMVERSION "${LIBZYPPGLIB_MAJOR} * 10000 + ${LIBZYPPGLIB_MINOR} * 100 + ${LIBZYPPGLIB_PATCH}")

set(LIBZYPPGLIB_VERSION         "${LIBZYPPGLIB_MAJOR}.${LIBZYPPGLIB_MINOR}.${LIBZYPPGLIB_PATCH}")
set(LIBZYPPGLIB_VERSION_INFO    "${LIBZYPPGLIB_SO_FIRST}.${LIBZYPPGLIB_AGE}.${LIBZYPPGLIB_PATCH}")
set(LIBZYPPGLIB_SOVERSION_INFO  "${LIBZYPPGLIB_SO_FIRST}")

message(STATUS "libzypp-glib version: ${LIBZYPPGLIB_VERSION}  soname: ${LIBZYPPGLIB_VERSION_INFO}")
