ADD_SUBDIRECTORY(base)

ADD_TESTS(
  Arch
  Capabilities
  CheckSum
  ContentType
  CpeId
  Date
  DrunkenBishop
  Dup
  Digest
  Deltarpm
  Edition
  ExtendedPool
  FileChecker
  Flags
  GZStream
  InstanceId
  KeyRing
  Locale
  Locks
  PathInfo
  Pathname
  PluginFrame
  PoolQueryCC
  PoolQuery
  ProgressData
  PtrTypes
  PublicKey
  PurgeKernels
  RWPtr
  RepoManager
  RepoStatus
  ResKind
  Resolver
  ResStatus
  RpmPkgSigCheck
  Selectable
  SetRelationMixin
  SetTracker
  StrMatcher
  StringV
  Target
  Url
  UserData
  Vendor
)

IF( NOT DISABLE_LIBPROXY )
  FIND_PACKAGE(libproxy)
  IF ( NOT LIBPROXY_FOUND )
    MESSAGE( FATAL_ERROR "libproxy not found" )
  ELSE ( NOT LIBPROXY_FOUND )

    include(CheckSymbolExists)

    # Check for function px_proxy_factory_free_proxies
    set( CMAKE_REQUIRED_INCLUDES ${LIBPROXY_INCLUDE_DIRS} )
    set( CMAKE_REQUIRED_LIBRARIES ${LIBPROXY_LIBRARIES} )
    check_symbol_exists(px_proxy_factory_free_proxies "proxy.h" LIBPROXY_HAVE_FREE_PROXIES)

    ADD_TESTS(
      Proxy
    )
    if(LIBPROXY_HAVE_FREE_PROXIES)
      target_compile_definitions( Proxy_test PUBLIC LIBPROXY_HAVE_FREE_PROXIES )
    endif()
    target_link_libraries( Proxy_test ${LIBPROXY_LIBRARIES} )
    target_include_directories( Proxy_test PUBLIC ${LIBPROXY_INCLUDE_DIRS} )
  ENDIF( NOT LIBPROXY_FOUND )
ENDIF()

IF (ENABLE_ZCHUNK_COMPRESSION)
  ADD_TESTS (
    ZChunk
  )
ENDIF(ENABLE_ZCHUNK_COMPRESSION)

IF( NOT DISABLE_MEDIABACKEND_TESTS )
  ADD_TESTS(
    Fetcher
    MediaSetAccess
    RepoInfo
  )
ENDIF()

