add_executable(test_unit test_unit.c 
    ${CMAKE_SOURCE_DIR}/shared/sourceValidator.c 
    ${CMAKE_SOURCE_DIR}/shared/jsonHandler.c
    ${CMAKE_SOURCE_DIR}/server/eventQueue.c
)
target_include_directories(test_unit PRIVATE ${CMAKE_SOURCE_DIR}/shared ${CMAKE_SOURCE_DIR}/server ${CMAKE_SOURCE_DIR}/cJSON-lib)
target_link_libraries(test_unit PRIVATE cJSON-lib pthread)
target_compile_definitions(test_unit PRIVATE INPUT_TXT_PATH="${CMAKE_SOURCE_DIR}/input.txt")

add_executable(test_network test_network.c
    ${CMAKE_SOURCE_DIR}/server/server.c
    ${CMAKE_SOURCE_DIR}/server/eventQueue.c
    ${CMAKE_SOURCE_DIR}/shared/sourceValidator.c
    ${CMAKE_SOURCE_DIR}/shared/jsonHandler.c
)
target_include_directories(test_network PRIVATE ${CMAKE_SOURCE_DIR}/shared ${CMAKE_SOURCE_DIR}/server ${CMAKE_SOURCE_DIR}/sockets-lib ${CMAKE_SOURCE_DIR}/cJSON-lib)
target_link_libraries(test_network PRIVATE sockets-lib cJSON-lib pthread)
target_compile_definitions(test_network PRIVATE INPUT_TXT_PATH="${CMAKE_SOURCE_DIR}/input.txt")

add_test(NAME UnitTests COMMAND test_unit)
add_test(NAME NetworkTests COMMAND test_network)

find_program(NODE_EXECUTABLE node)
if (NODE_EXECUTABLE)
    add_test(NAME GatewayTests COMMAND ${NODE_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gateway/test_gateway.js)
    set_tests_properties(GatewayTests PROPERTIES ENVIRONMENT "SERVER_BIN=$<TARGET_FILE:server>")
endif()
