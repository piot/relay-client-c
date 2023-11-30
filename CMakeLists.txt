cmake_minimum_required(VERSION 3.16.3)
project(relay-client C)

add_subdirectory(deps/piot/clog/src/lib)
add_subdirectory(deps/piot/datagram-transport-c/src/lib)
add_subdirectory(deps/piot/discoid-c/src/lib)
add_subdirectory(deps/piot/flood-c/src/lib)
add_subdirectory(deps/piot/imprint/src/lib)
add_subdirectory(deps/piot/monotonic-time-c/src/lib)
add_subdirectory(deps/piot/relay-serialize-c/src/lib)
add_subdirectory(deps/piot/tiny-libc/src/lib)


add_subdirectory(src)
#add_subdirectory(deps/piot/guise-client-c/src/lib)
#add_subdirectory(deps/piot/udp-client-c/src/lib)
