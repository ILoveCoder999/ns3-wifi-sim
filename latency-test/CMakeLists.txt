build_exec (
    EXECNAME latency-test
    SOURCE_FILES interference-application.cc interference-application-helper.cc latency-test.cc
    LIBRARIES_TO_LINK ${ns3-libs} nlohmann_json::nlohmann_json
    # use "${ns3-libs}" "${ns3-contrib-libs}" in case you want to link to all
    # enabled modules
    EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/latency-test
)