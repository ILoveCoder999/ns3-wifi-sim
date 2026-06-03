build_lib_example(
  NAME mesh-sim
  SOURCE_FILES mesh-sim.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
)

build_lib_example(
  NAME mesh3d-sim
  SOURCE_FILES mesh3d-sim.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libtraffic-control}
)
build_lib_example(
  NAME mesh4dw6-sim
  SOURCE_FILES mesh4dw6-sim.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libpoint-to-point}
    ${libapplications}
    ${libtraffic-control}
)

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