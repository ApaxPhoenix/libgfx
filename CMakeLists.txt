cmake_minimum_required(VERSION 4.2)
project(libgfx VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(GFX_VULKAN "Build the headless 3D renderer" ON)
option(GFX_EXAMPLES "Build the libgfx demo (needs GFX_VULKAN)" "${PROJECT_IS_TOP_LEVEL}")
option(GFX_CLANG_TIDY "Run clang-tidy on every file as it compiles" OFF)

if(GFX_CLANG_TIDY)
    find_program(CLANG_TIDY NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}")
endif()

if(MSVC)
    set(GFX_WARNINGS /W4 /permissive-)
else()
    set(GFX_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Library

find_package(unofficial-skia CONFIG REQUIRED)

add_library(gfx STATIC
        src/image/image.cpp
        src/canvas/canvas.cpp
)
add_library(gfx::gfx ALIAS gfx)
target_compile_features(gfx PUBLIC cxx_std_26)
target_include_directories(gfx PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(gfx PRIVATE unofficial::skia::skia)
target_compile_options(gfx PRIVATE ${GFX_WARNINGS})

if(GFX_VULKAN)
    find_package(Vulkan 1.1 REQUIRED)
    find_program(SLANGC NAMES slangc REQUIRED)
    include(shaders)

    target_sources(gfx PRIVATE src/scene/renderer.cpp)
    gfx_shaders(gfx "${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.slang" vertex fragment)
    target_link_libraries(gfx PRIVATE Vulkan::Vulkan)
endif()

# Demo: writes frame.png, or opens a preview window

if(GFX_EXAMPLES AND GFX_VULKAN)
    include(glfw)
    find_package(OpenGL REQUIRED)

    add_executable(libgfx src/main.cpp)
    target_link_libraries(libgfx PRIVATE gfx::gfx glfw OpenGL::GL)
    target_compile_options(libgfx PRIVATE ${GFX_WARNINGS})
    if(APPLE)
        set_property(TARGET libgfx APPEND PROPERTY BUILD_RPATH "$<TARGET_FILE_DIR:Vulkan::Vulkan>")
        target_compile_definitions(libgfx PRIVATE GL_SILENCE_DEPRECATION)
    endif()

    include(CTest)
    if(BUILD_TESTING)
        add_test(NAME libgfx COMMAND libgfx --check)
    endif()
endif()
