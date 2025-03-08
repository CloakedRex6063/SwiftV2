FILE(GLOB_RECURSE COMMON_SOURCES Source/*.cpp)
add_library(Common ${COMMON_SOURCES})
target_include_directories(Common PUBLIC Include)
target_link_libraries(Common PRIVATE Swift::V2 DDS)