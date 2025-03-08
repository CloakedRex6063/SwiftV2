FetchContent_Declare(
        dds
        GIT_REPOSITORY https://github.com/CloakedRex6063/DDS-Utility
        GIT_TAG bb4d44e4
)
FetchContent_MakeAvailable(dds)
target_link_libraries(Swift PUBLIC DDS)

include(Scripts/functions.cmake)
add_subdirectory(Common)
add_subdirectory(Showcase)
