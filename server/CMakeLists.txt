add_executable(server)
target_sources(server PRIVATE main.c server.c eventQueue.c ${CMAKE_SOURCE_DIR}/shared/sourceValidator.c ${CMAKE_SOURCE_DIR}/shared/jsonHandler.c)
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/shared ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(server PRIVATE sockets-lib cJSON-lib pthread)
