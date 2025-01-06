add_executable(server)
target_sources(server PRIVATE main.c)
target_link_libraries(server PRIVATE sockets-lib)
