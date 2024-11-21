build_exec (
    EXECNAME my-test-simulation
    SOURCE_FILES my-test-application.cc my-test-application-helper.cc my-test-simulation.cc
    LIBRARIES_TO_LINK ${ns3-libs}
    # use "${ns3-libs}" "${ns3-contrib-libs}" in case you want to link to all
    # enabled modules
    EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/my-test-simulation
)