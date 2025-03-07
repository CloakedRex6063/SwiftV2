cmake_minimum_required(VERSION 3.30)
project(SwiftV2)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

include(FetchContent)

option(SWIFT_EXAMPLES "Build examples" OFF)
option(SWIFT_GLFW "Use GLFW" ON)
option(SWIFT_IMGUI "Use ImGui" ON)

FILE(GLOB_RECURSE SWIFT_SOURCES Source/*.cpp)
add_library(Swift ${SWIFT_SOURCES})
add_library(Swift::V2 ALIAS Swift)
target_include_directories(Swift PUBLIC Include)

add_subdirectory(External)
if(SWIFT_EXAMPLES)
    add_subdirectory(Examples)
endif ()

target_compile_definitions(Swift PRIVATE
        $<$<CONFIG:Debug>:SWIFT_DEBUG>
        $<$<CONFIG:Release>:SWIFT_RELEASE>
)

if(SWIFT_GLFW)
    target_compile_definitions(Swift PRIVATE SWIFT_GLFW)
endif()
if (SWIFT_IMGUI)
    target_compile_definitions(Swift PRIVATE SWIFT_IMGUI)
endif()




