aux_source_directory(. base_SRCS)
add_library(base ${base_SRCS} util.cpp util.h)
add_subdirectory(benchmark)
target_link_libraries(base pthread benchmark)



