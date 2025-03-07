function(add_game NAME SOURCE_DIR BINARY_DIR)
    FILE(GLOB_RECURSE SOURCES ${SOURCE_DIR}/Source/*.cpp)
    add_executable(${NAME} ${SOURCES})
    target_include_directories(${NAME} PUBLIC ${SOURCE_DIR}/Include)
    target_link_libraries(${NAME} PUBLIC Swift::V2 glfw imgui)
    compile_shaders(${NAME}_SHADERS ${SOURCE_DIR}/../Shaders/ ${BINARY_DIR}/Shaders/)
    add_dependencies(${NAME} ${NAME}_SHADERS)
endfunction()

function(copy_resources NAME SRC_DIR DST_DIR)
    add_custom_target(${NAME}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DST_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${SRC_DIR} ${DST_DIR}
            COMMENT "Copying resources from ${SRC_DIR} to ${DST_DIR}"
    )
endfunction()

function(compile_shaders NAME PATH OUT_PATH)
    file(GLOB_RECURSE GLSL_SOURCE_FILES
            "${PATH}/*.frag"
            "${PATH}/*.vert"
            "${PATH}/*.comp")
    find_program(GLSL_VALIDATOR glslc HINTS
            $ENV{VULKAN_SDK}/Bin
            "C:/VulkanSDK/1.4.304.1/Bin"
            "D:/VulkanSDK/1.4.304.1/Bin"
            REQUIRED)
    message(STATUS "Gathering shaders")
    foreach(GLSL ${GLSL_SOURCE_FILES})
        get_filename_component(FILE_NAME ${GLSL} NAME)
        set(SPIRV "${OUT_PATH}/${FILE_NAME}.spv")
        message(STATUS ${GLSL})
        add_custom_command(
                OUTPUT ${SPIRV}
                COMMAND ${GLSL_VALIDATOR} ${GLSL} --target-env=vulkan1.3 -o ${SPIRV} -g
                DEPENDS ${GLSL})
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach ()
    add_custom_target(${NAME} DEPENDS ${SPIRV_BINARY_FILES})
endfunction()