add_executable(client)
target_sources(client PRIVATE main.c terminal.c game.c sourceValidator.c inputHandler.c cJSON.c jsonHandler.c)
target_link_libraries(client PRIVATE sockets-lib)
