PROJECT( zyppng C CXX )
set (CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_EXTENSIONS OFF)

pkg_check_modules ( LIBGLIB REQUIRED glib-2.0 )
INCLUDE_DIRECTORIES( ${LIBGLIB_INCLUDE_DIRS} )

pkg_check_modules ( LIBGOBJECT REQUIRED gobject-2.0 )
INCLUDE_DIRECTORIES( ${LIBGOBJECT_INCLUDE_DIRS} )

pkg_check_modules ( LIBGIO REQUIRED gio-2.0 )
INCLUDE_DIRECTORIES( ${LIBGIO_INCLUDE_DIRS} )

include(GObjectIntrospection)
include(GLibTools)

# Collect all files that may contain translatable texts
FILE( GLOB_RECURSE POT_FILE_DEPENDS RELATIVE ${LIBZYPP_SOURCE_DIR} "*.h" "*.cc" )
SET( POT_FILE_DEPENDS_ZYPPNG ${POT_FILE_DEPENDS} PARENT_SCOPE )

INCLUDE_DIRECTORIES ( ${LIBZYPP_SOURCE_DIR} ${CMAKE_BINARY_DIR} )

ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

SET( zyppng_HEADERS
  context.h
  downloader.h
  expected.h
  infobase.h
  repoinfo.h
  repomanager.h
  repository.h
  serviceinfo.h
  zyppng.h
)

SET( zyppng_private_HEADERS
  private/context_p.h
  private/downloader_p.h
  private/expected_p.h
  private/globals_p.h
  private/infobase_p.h
  private/repoinfo_p.h
  private/repomanager_p.h
  private/repository_p.h
  private/serviceinfo_p.h
)

SET( zyppng_SRCS
  context.cc
  downloader.cc
  expected.cc
  globals.cc
  infobase.cc
  repoinfo.cc
  repomanager.cc
  repository.cc
  serviceinfo.cc
)

glib_mkenums( zyppenums zyppenums.h.in zyppenums.c.in ${zyppng_HEADERS} )

SET( zyppng_public_HEADERS ${zyppng_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/zyppenums.h )

INSTALL(  FILES ${zyppng_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zyppng" )

SET( zyppng_utils_HEADERS
  utils/GioMemory
  utils/GLibMemory
  utils/GList
  utils/GObjectMemory
  utils/OpaqueTypePtr
  utils/ResourcePtr
  utils/RetainPtr
)

SET( zyppng_utils_SRCS
)

INSTALL(  FILES ${zyppng_utils_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zyppng/utils" )

SET( zyppng_lib_SRCS
    ${zyppng_SRCS}
    ${zyppng_utils_SRCS}
    ${CMAKE_CURRENT_BINARY_DIR}/zyppenums.c
)

SET( zyppng_lib_HEADERS
    ${zyppng_private_HEADERS} ${zyppng_public_HEADERS} ${zyppng_utils_HEADERS}
)

# Default loggroup for all files
SET_LOGGROUP( "zyppng" ${zyppng_lib_SRCS} )

ADD_LIBRARY( zyppng SHARED  ${zyppng_lib_SRCS} ${zyppng_lib_HEADERS} )
TARGET_LINK_LIBRARIES( zyppng zypp )
target_link_libraries( zyppng ${LIBGOBJECT_LIBRARIES} )
target_link_libraries( zyppng ${LIBGIO_LIBRARIES} )

gobject_introspection(
  FILENAME Zypp-1.0.gir
  PACKAGES glib-2.0 gio-2.0 gobject-2.0
  NAMESPACE Zypp
  LIBRARY zyppng
  #QUIET
  SCANNER_ARGS --add-include-path=${CMAKE_CURRENT_SOURCE_DIR}
               --include=GLib-2.0 --include=GObject-2.0 --include=Gio-2.0
  COMPILER_ARGS --includedir=${CMAKE_CURRENT_SOURCE_DIR}
  SYMBOL_PREFIXES zypp
  SOURCES ${zyppng_public_HEADERS}
  #BUILT_SOURCES ${ZYPPNG_PUBLIC_BUILT_SOURCES}
  #              ${ZYPPNG_PUBLIC_BUILT_HEADERS}
)
