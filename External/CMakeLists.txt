find_package(Vulkan REQUIRED)
if(Vulkan_FOUND)
    message(STATUS "Vulkan SDK is installed")
    target_compile_definitions(Swift PRIVATE SWIFT_VULKAN_SDK VULKAN_HPP_NO_EXCEPTIONS)
    target_link_libraries(Swift PRIVATE Vulkan::Vulkan)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "Enabling validation layers")
        target_compile_definitions(Swift PRIVATE SWIFT_VULKAN_VALIDATION)
    endif ()
else()
    message(FATAL_ERROR "Vulkan SDK not found!")
endif()

FetchContent_Declare(
        VKB
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
        GIT_TAG de3c684
)
FetchContent_MakeAvailable(VKB)
target_link_libraries(Swift PUBLIC vk-bootstrap::vk-bootstrap)

FetchContent_Declare(
        Volk
        GIT_REPOSITORY https://github.com/zeux/volk
        GIT_TAG 58689c0
)
FetchContent_MakeAvailable(Volk)
target_link_libraries(Swift PRIVATE volk::volk)

if(SWIFT_GLFW)
    FetchContent_Declare(
            GLFW
            GIT_REPOSITORY https://github.com/glfw/glfw
            GIT_TAG e7ea71b
    )
    FetchContent_MakeAvailable(GLFW)
    target_link_libraries(Swift PRIVATE glfw)
endif ()

if(SWIFT_IMGUI)
    FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui
            GIT_TAG v1.91.7-docking
    )
    FetchContent_MakeAvailable(imgui)

    find_package(Vulkan REQUIRED)
    FILE(GLOB ImGuiSources ${imgui_SOURCE_DIR}/*.cpp)
    add_library(imgui ${ImGuiSources})
    target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(imgui PRIVATE Vulkan::Vulkan volk::volk)
    target_sources(imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
    target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
    if(SWIFT_GLFW)
        target_sources(imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
        target_link_libraries(imgui PRIVATE glfw)
    endif ()
    target_link_libraries(Swift PRIVATE imgui)
endif ()

FetchContent_Declare(
        VMA
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
        GIT_TAG c788c52
)
FetchContent_MakeAvailable(VMA)
if (MSVC)
    target_compile_options(VulkanMemoryAllocator INTERFACE /W0)
else()
    target_compile_options(VulkanMemoryAllocator INTERFACE -w)
endif()
target_link_libraries(Swift PUBLIC GPUOpen::VulkanMemoryAllocator)
