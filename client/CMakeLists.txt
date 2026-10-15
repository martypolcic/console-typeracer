add_executable(client)
target_sources(client PRIVATE main.c client.c ${CMAKE_SOURCE_DIR}/shared/sourceValidator.c ${CMAKE_SOURCE_DIR}/shared/jsonHandler.c ${CMAKE_SOURCE_DIR}/shared/terminal.c)
target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/shared ${CMAKE_SOURCE_DIR}/sockets-lib)
target_link_libraries(client PRIVATE sockets-lib cJSON-lib pthread)