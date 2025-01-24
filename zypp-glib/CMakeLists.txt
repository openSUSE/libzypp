PROJECT( zypp-glib C CXX )
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
SET( POT_FILE_DEPENDS_ZYPP_GLIB ${POT_FILE_DEPENDS} PARENT_SCOPE )

INCLUDE_DIRECTORIES ( ${LIBZYPP_SOURCE_DIR} ${LIBZYPP_BINARY_DIR} )

ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

SET( zypp_glib_HEADERS
  application.h
  context.h
  error.h
  expected.h
  infobase.h
  managedfile.h
  progressobserver.h
  repoinfo.h
  repomanager.h
  repository.h
  serviceinfo.h
  zypp-glib.h

)

SET( zypp_glib_ui_HEADERS
  ui/booleanchoicerequest.h
  ui/inputrequest.h
  ui/listchoicerequest.h
  ui/showmessagerequest.h
  ui/userrequest.h

)

SET( zypp_glib_private_HEADERS
  private/cancellable_p.h
  private/context_p.h
  private/error_p.h
  private/expected_p.h
  private/globals_p.h
  private/infobase_p.h
  private/managedfile_p.h
  private/progressobserver_p.h
  private/repoinfo_p.h
  private/repomanager_p.h
  private/repository_p.h
  private/serviceinfo_p.h
  private/callbacks/legacyreportreceiverbase.h
  #private/callbacks/keyring.h
  #private/callbacks/media.h
  #private/callbacks/rpm.h
  private/callbacks/repo.h
  private/callbacks/job.h
  ui/private/booleanchoicerequest_p.h
  ui/private/inputrequest_p.h
  ui/private/listchoicerequest_p.h
  ui/private/showmessagerequest_p.h
  ui/private/userrequest_p.h
)

SET( zypp_glib_SRCS
  application.cc
  context.cc
  error.cc
  expected.cc
  globals.cc
  infobase.cc
  managedfile.cc
  progressobserver.cc
  repoinfo.cc
  repomanager.cc
  repository.cc
  serviceinfo.cc
  zypp-glib.cc
  private/cancellable_p.cc
  ui/booleanchoicerequest.cc
  ui/inputrequest.cc
  ui/listchoicerequest.cc
  ui/showmessagerequest.cc
  ui/userrequest.cc
)

glib_mkenums( zyppenums zyppenums.h.in zyppenums.c.in ${zypp_glib_HEADERS} )
glib_mkenums( ui/zyppenums-ui ui/zyppenums-ui.h.in ui/zyppenums-ui.c.in ${zypp_glib_ui_HEADERS} )

INSTALL(  FILES ${zypp_glib_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/zyppenums.h  DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-glib" )
INSTALL(  FILES ${zypp_glib_ui_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/ui/zyppenums-ui.h DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-glib/ui" )

SET( zypp_glib_public_HEADERS ${zypp_glib_HEADERS} ${zypp_glib_ui_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/zyppenums.h ${CMAKE_CURRENT_BINARY_DIR}/ui/zyppenums-ui.h )

SET( zypp_glib_utils_HEADERS
  utils/GLibMemory
  utils/GList
  utils/GObjectMemory
  utils/GioMemory
  utils/OpaqueTypePtr
  utils/ResourcePtr
  utils/RetainPtr
)

SET( zypp_glib_utils_SRCS
)

INSTALL(  FILES ${zypp_glib_utils_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-glib/utils" )

SET( zypp_glib_lib_SRCS
    ${zypp_glib_SRCS}
    ${zypp_glib_utils_SRCS}
    ${CMAKE_CURRENT_BINARY_DIR}/zyppenums.c
    ${CMAKE_CURRENT_BINARY_DIR}/ui/zyppenums-ui.c
)

SET( zypp_glib_lib_HEADERS
    ${zypp_glib_private_HEADERS}
    ${zypp_glib_HEADERS}
    ${zypp_glib_ui_HEADERS}
    ${zypp_glib_utils_HEADERS}
    ${CMAKE_CURRENT_BINARY_DIR}/zyppenums.h
    ${CMAKE_CURRENT_BINARY_DIR}/ui/zyppenums-ui.h
)

# Default loggroup for all files
SET_LOGGROUP( "zyppng" ${zypp_glib_lib_SRCS} )

ADD_LIBRARY( zypp-glib SHARED  ${zypp_glib_lib_SRCS} ${zypp_glib_lib_HEADERS} )
target_link_libraries( zypp-glib zypp_lib_compiler_flags )
TARGET_LINK_LIBRARIES( zypp-glib zypp-allsym )
target_link_libraries( zypp-glib ${LIBGOBJECT_LIBRARIES} )
target_link_libraries( zypp-glib ${LIBGIO_LIBRARIES} )

INSTALL(TARGETS zypp-glib LIBRARY DESTINATION ${LIB_INSTALL_DIR} )

INCLUDE(GenerateExportHeader)
message("Exporting experimental libzypp-ng API")
GENERATE_EXPORT_HEADER(
  zypp-glib
  BASE_NAME zypp-glib
  PREFIX_NAME LIB
)

gobject_introspection(
  FILENAME Zypp-1.0.gir
  PACKAGES glib-2.0 gobject-2.0 gio-2.0
  NAMESPACE Zypp
  LIBRARY zypp-glib
  #QUIET
  SCANNER_ARGS --add-include-path=${CMAKE_CURRENT_SOURCE_DIR}
               --include=GLib-2.0 --include=GObject-2.0 --include=Gio-2.0
  COMPILER_ARGS --includedir=${CMAKE_CURRENT_SOURCE_DIR}
  SYMBOL_PREFIXES zypp
  SOURCES ${zypp_glib_public_HEADERS}
  #BUILT_SOURCES ${zypp_glib_PUBLIC_BUILT_SOURCES}
  #              ${zypp_glib_PUBLIC_BUILT_HEADERS}
)
