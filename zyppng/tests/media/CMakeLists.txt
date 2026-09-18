IF( NOT DISABLE_MEDIABACKEND_TESTS)
  ADD_TESTS(
    Provider
    ProvideMessage
 )
  target_link_libraries( Provider_test PUBLIC tvm-protocol-obj )
ENDIF()
