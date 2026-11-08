# GLFW for the preview window in src/main.cpp: an installed 3.4+ if there is one, otherwise a
# pinned, hash-checked download.

find_package(glfw3 3.4 CONFIG QUIET)
if(NOT TARGET glfw)
    include(FetchContent)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
            glfw
            URL https://github.com/glfw/glfw/archive/3.5.1.tar.gz
            URL_HASH SHA512=42e8b8bdc2ecaab8d33d43cab6cf6e4bfd1f23492a5f13ec4b3762b74661dc3c9625721f436fbf841909b9af3ac6dcb8080201b36592d33c2bb196f768fc307f
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(glfw)
endif()
