build_exec (
    EXECNAME fdr-error-rate-model
    SOURCE_FILES my-packets-error-model.cc simulation.cc
    LIBRARIES_TO_LINK ${ns3-libs}
    # use "${ns3-libs}" "${ns3-contrib-libs}" in case you want to link to all
    # enabled modules
    EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/fdr-error-rate-model
)