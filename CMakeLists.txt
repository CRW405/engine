cmake_minimum_required(VERSION 3.25)

project(oxide VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Development defaults stay on. Disable these for lean release builds.
option(OXIDE_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)
option(OXIDE_ENABLE_LOGGING "Enable Oxide diagnostic logging" ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)

find_path(STB_INCLUDE_DIR stb_image.h
    PATHS /usr/include/stb
)
if(NOT STB_INCLUDE_DIR)
    message(FATAL_ERROR "stb headers not found!")
endif()
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${STB_INCLUDE_DIR})

function(oxide_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror -DGLFW_USE_WAYLAND=ON)
    endif()
endfunction()

# Oxide itself is a library that example programs (and other projects)
# can link against. Every source file under src/ is part of it; applications
# live in examples/.
file(GLOB_RECURSE OXIDE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_library(oxide STATIC ${OXIDE_SOURCES})
add_library(oxide::oxide ALIAS oxide)

target_include_directories(oxide PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/glfw"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan"
)
target_link_libraries(oxide PUBLIC Vulkan::Vulkan glfw stb)
target_compile_definitions(oxide PUBLIC
    OXIDE_ENABLE_VALIDATION=$<BOOL:${OXIDE_ENABLE_VALIDATION}>
    OXIDE_ENABLE_LOGGING=$<BOOL:${OXIDE_ENABLE_LOGGING}>
)

oxide_enable_warnings(oxide)

message(STATUS "Oxide validation layers: ${OXIDE_ENABLE_VALIDATION}")
message(STATUS "Oxide diagnostic logging: ${OXIDE_ENABLE_LOGGING}")

option(OXIDE_BUILD_EXAMPLES "Build the example programs" ON)
if(OXIDE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
