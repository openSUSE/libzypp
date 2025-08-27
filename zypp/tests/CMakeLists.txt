INCLUDE_DIRECTORIES( ${LIBZYPP_SOURCE_DIR} ${LIBZYPP_BINARY_DIR} ${zypp-libs_SOURCE_DIR}/zypp-logic )
ADD_DEFINITIONS( -DTESTS_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}" -DTESTS_SHARED_DIR="${zypp-libs_SOURCE_DIR}/zypp-logic/tests" -DTESTS_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}" )

if(TRUE)
  SET(TESTLIBNAME zypp_test_utils )
  SET(ZYPP_COMP_FLAGS zypp_lib_compiler_flags)
  ADD_SUBDIRECTORY(${zypp-libs_SOURCE_DIR}/zypp-logic/tests/lib ${CMAKE_CURRENT_BINARY_DIR}/lib )
endif()

MACRO(ADD_TESTS)
  FOREACH( loop_var ${ARGV} )
    SET_SOURCE_FILES_PROPERTIES( ${loop_var}_test.cc COMPILE_FLAGS "-DBOOST_TEST_DYN_LINK -DBOOST_TEST_MAIN -DBOOST_AUTO_TEST_MAIN=\"\" " )
    ADD_EXECUTABLE( ${loop_var}_test ${loop_var}_test.cc )

    SET(TEST_REQ_LIBS ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} )
    LIST( APPEND TEST_REQ_LIBS zypp_test_utils )

    TARGET_LINK_LIBRARIES( ${loop_var}_test PUBLIC zypp_exe_compiler_flags  )
    TARGET_LINK_LIBRARIES( ${loop_var}_test PUBLIC ${TEST_REQ_LIBS}  )
    TARGET_LINK_LIBRARIES( ${loop_var}_test PUBLIC zypp-allsym  )
    ADD_TEST( ${loop_var}_test ${CMAKE_CURRENT_BINARY_DIR}/${loop_var}_test --catch_system_errors=no)
  ENDFOREACH( loop_var )
ENDMACRO(ADD_TESTS)

ENABLE_TESTING()

ADD_SUBDIRECTORY( media )
ADD_SUBDIRECTORY( zypp )
ADD_SUBDIRECTORY( parser )
ADD_SUBDIRECTORY( repo )
ADD_SUBDIRECTORY( sat )
ADD_SUBDIRECTORY( zyppng )

ADD_CUSTOM_TARGET( ctest
   COMMAND ctest -VV -a
)
