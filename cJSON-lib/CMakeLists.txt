add_library(cJSON-lib SHARED)
target_sources(cJSON-lib PRIVATE cJSON.c)
target_compile_options(cJSON-lib PRIVATE "-Wall" "-Wextra" "-Wpedantic")

