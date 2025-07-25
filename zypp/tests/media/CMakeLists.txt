ADD_TESTS(
    CredentialManager
    CredentialFileReader
    MediaProducts
    MetaLinkParser
)
IF( NOT DISABLE_MEDIABACKEND_TESTS )
  ADD_TESTS(
    MediaCurl
  )
ENDIF()
