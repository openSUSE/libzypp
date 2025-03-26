ADD_EXECUTABLE( zypp-testrunner
  main.cc
)
TARGET_LINK_LIBRARIES( zypp-testrunner
  zypp
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
)

INSTALL(TARGETS zypp-testrunner DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
