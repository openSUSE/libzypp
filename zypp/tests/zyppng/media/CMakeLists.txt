IF( NOT DISABLE_MEDIABACKEND_TESTS)
  ADD_TESTS(
    NetworkRequestDispatcher
    #EvDownloader
    Provider
  )
  target_link_libraries( Provider_test PUBLIC tvm-protocol-obj )
ENDIF()

