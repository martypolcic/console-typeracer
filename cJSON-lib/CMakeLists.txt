add_library(cJSON-lib STATIC)
target_sources(cJSON-lib PRIVATE cJSON.c)
target_compile_options(cJSON-lib PRIVATE "-Wall" "-Wextra" "-Wpedantic")

