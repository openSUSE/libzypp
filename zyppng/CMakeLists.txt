project(zyppng)

SET ( CMAKE_CXX_STANDARD 20 )
SET ( CMAKE_CXX_EXTENSIONS OFF )


SET ( zyppng_lib_SRCS )

# Default loggroup for all files
#SET_LOGGROUP( "zypp" ${zyppng_lib_SRCS} )

if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )
  message(NOTICE "Enabling static analysis: ${ZYPP_CXX_CLANG_TIDY} ${ZYPP_CXX_CPPCHECK}")

  if (ZYPP_CXX_CLANG_TIDY)
    set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
  endif(ZYPP_CXX_CLANG_TIDY)

  if (ZYPP_CXX_CPPCHECK)
    set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
  endif(ZYPP_CXX_CPPCHECK)
endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

add_subdirectory( lib )
add_subdirectory( api )
add_subdirectory( tools )
add_subdirectory( tests )

# this part does nothing yet, only when switching away from using a map file to
# a approach where symbols are exported explicitly it would make a difference
# ZYPPNG symbols need to be added to the map file to be exported
# IF( EXPORT_NG_API )
# ADD_DEFINITIONS(-DEXPORT_EXPERIMENTAL_API)
# INCLUDE(GenerateExportHeader)
# message("Exporting experimental libzypp-ng API")
# GENERATE_EXPORT_HEADER(
#   zypp
#   BASE_NAME zypp-ng
#   PREFIX_NAME LIB
# )
# ENDIF ( EXPORT_NG_API )
