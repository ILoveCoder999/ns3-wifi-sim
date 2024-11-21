build_exec (
    EXECNAME latency-test-v2
    SOURCE_FILES interferer-application.cc interferer-application-helper.cc my-udp-client.cc my-udp-client-helper.cc utils.cc latency-test.cc
    LIBRARIES_TO_LINK ${ns3-libs} nlohmann_json::nlohmann_json
    # use "${ns3-libs}" "${ns3-contrib-libs}" in case you want to link to all
    # enabled modules
    EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/latency-test-v2
)