add_library(sockets-lib STATIC)
target_sources(sockets-lib PRIVATE socket.c)
target_compile_options(sockets-lib PRIVATE "-Wall" "-Wextra" "-Wpedantic")

