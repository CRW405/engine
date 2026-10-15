cmake_minimum_required(VERSION 3.25)

project(engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
find_program(GLSLC_EXECUTABLE glslc)
find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator)

if(GLSLC_EXECUTABLE)
    set(SHADER_COMPILER "${GLSLC_EXECUTABLE}")
    set(SHADER_COMPILER_ARGS "")
elseif(GLSLANG_VALIDATOR_EXECUTABLE)
    set(SHADER_COMPILER "${GLSLANG_VALIDATOR_EXECUTABLE}")
    set(SHADER_COMPILER_ARGS "-V")
else()
    message(FATAL_ERROR "Neither glslc nor glslangValidator was found")
endif()

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/*.vert"
    "${SHADER_SOURCE_DIR}/*.frag"
    "${SHADER_SOURCE_DIR}/*.comp"
)

set(SHADER_OUTPUTS)
foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
    get_filename_component(SHADER_NAME "${SHADER_SOURCE}" NAME)
    set(SHADER_OUTPUT "${SHADER_BINARY_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT "${SHADER_OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_BINARY_DIR}"
        COMMAND "${SHADER_COMPILER}" ${SHADER_COMPILER_ARGS}
                "${SHADER_SOURCE}" -o "${SHADER_OUTPUT}"
        DEPENDS "${SHADER_SOURCE}"
        COMMENT "Compiling ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND SHADER_OUTPUTS "${SHADER_OUTPUT}")
endforeach()

add_custom_target(shaders DEPENDS ${SHADER_OUTPUTS})

file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp" "src/**/*.cpp")
add_executable(engine ${ENGINE_SOURCES})
add_dependencies(engine shaders)

target_include_directories(engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/glfw"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan"
)
target_link_libraries(engine PRIVATE Vulkan::Vulkan glfw)

if(MSVC)
    target_compile_options(engine PRIVATE /W4)
else()
    target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_custom_target(run
    COMMAND engine
    DEPENDS engine
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Running engine"
)
