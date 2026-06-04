
build_lib_example(
  NAME fat-tree-sim
  SOURCE_FILES fat-tree-sim.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libtraffic-control}
)






build_lib_example(
  NAME rng-sim
  SOURCE_FILES rng-sim.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libtraffic-control}
)
