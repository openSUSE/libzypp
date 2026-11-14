ADD_TESTS(
    CredentialManager
    CredentialFileReader
    MediaProducts
    MetaLinkParser
    MediaNetworkRequestExecutor
)
IF( NOT DISABLE_MEDIABACKEND_TESTS )
  ADD_TESTS(
    MediaCurl
    MediaNetworkRequestExecutorWebServer
  )
  SET_TESTS_PROPERTIES( MediaNetworkRequestExecutorWebServer_test PROPERTIES TIMEOUT 60 )
ENDIF()

# The stall/cancel tests must fail rather than hang the suite if cancellation regresses.
SET_TESTS_PROPERTIES( MediaNetworkRequestExecutor_test PROPERTIES TIMEOUT 60 )
