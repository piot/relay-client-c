# generated by cmake-generator
cmake_minimum_required(VERSION 3.16.3)

add_library(relay-client STATIC 
  client.c
  connector.c
  debug.c
  listener.c
  socket.c)

include(Tornado.cmake)
set_tornado(relay-client)

target_include_directories(relay-client PUBLIC ../include)


target_link_libraries(relay-client PUBLIC 
  relay-serialize
  datagram-transport
  monotonic-time
  discoid)

