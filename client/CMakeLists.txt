add_executable(client)
target_sources(client PRIVATE main.c)
target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/shared)
target_link_libraries(client PRIVATE sockets-lib cJSON-lib)

add_executable(client_test)
target_sources(client_test PRIVATE game.c ${CMAKE_SOURCE_DIR}/shared/sourceValidator.c ${CMAKE_SOURCE_DIR}/shared/jsonHandler.c ${CMAKE_SOURCE_DIR}/shared/inputHandler.c ${CMAKE_SOURCE_DIR}/shared/terminal.c)
target_include_directories(client_test PRIVATE ${CMAKE_SOURCE_DIR}/shared)
target_link_libraries(client_test PRIVATE sockets-lib cJSON-lib)