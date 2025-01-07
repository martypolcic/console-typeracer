cmake_minimum_required(VERSION 3.22)

project(
  typeracer
  VERSION     1
  DESCRIPTION "Typeracer semestralna praca Samuel Fabry & Martin Polcic"
  LANGUAGES   C
)

add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(sockets-lib)
add_subdirectory(cJSON-lib)
