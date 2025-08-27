
INCLUDE_DIRECTORIES( ${zyppng_SOURCE_DIR}/lib ${zyppng_BINARY_DIR}/lib ${zypp-libs_SOURCE_DIR}/zypp-logic )
ADD_DEFINITIONS( -DTESTS_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}"  -DTESTS_SHARED_DIR="${zypp-libs_SOURCE_DIR}/zypp-logic/tests" -DTESTS_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}" -DZYPPNG_WORKERS_DIR="${ZYPPNG_WORKER_BINARY_DIR}" )

if(TRUE)
  SET(TESTLIBNAME zyppng_test_utils )
  SET(ZYPP_COMP_FLAGS zyppng_lib_compiler_flags)
  ADD_SUBDIRECTORY(${zypp-libs_SOURCE_DIR}/zypp-logic/tests/lib ${CMAKE_CURRENT_BINARY_DIR}/lib )
endif()

MACRO(ADD_TESTS)
  FOREACH( loop_var ${ARGV} )
    SET_SOURCE_FILES_PROPERTIES( ${loop_var}_test.cc COMPILE_FLAGS "-DBOOST_TEST_DYN_LINK -DBOOST_TEST_MAIN -DBOOST_AUTO_TEST_MAIN=\"\" " )
    ADD_EXECUTABLE( ${loop_var}_test ${loop_var}_test.cc )

    SET(TEST_REQ_LIBS ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )
    LIST( APPEND TEST_REQ_LIBS zyppng_test_utils )

    TARGET_LINK_LIBRARIES( ${loop_var}_test PUBLIC zyppng_exe_compiler_flags  )
    TARGET_LINK_LIBRARIES( ${loop_var}_test PUBLIC ${TEST_REQ_LIBS}  )
    TARGET_LINK_LIBRARIES( ${loop_var}_test PUBLIC zyppng-allsym  )
    ADD_TEST( ${loop_var}_test ${CMAKE_CURRENT_BINARY_DIR}/${loop_var}_test --catch_system_errors=no)
  ENDFOREACH( loop_var )
ENDMACRO(ADD_TESTS)

add_subdirectory(media)
