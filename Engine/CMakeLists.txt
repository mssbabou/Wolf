cmake_minimum_required(VERSION 3.24)
project(Engine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the exe doesn't need the VC++ redistributable installed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# Link SDL statically so the build is a single exe (no SDL3.dll)
find_package(SDL3 CONFIG QUIET COMPONENTS SDL3-static)
if (NOT SDL3_FOUND)
    message(STATUS "SDL3 not found on system, fetching it")
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.4.18
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif ()

FetchContent_Declare(
        imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b-docking.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)

# stb_image (header-only PNG/JPG loader), pinned to a commit since stb has no release tags
FetchContent_Declare(
        stb
        URL https://github.com/nothings/stb/archive/6b5799bae8a1fe3792691ef4b72ae26434c3e799.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(stb)

# ---------- Engine ----------
# Pick up every source file automatically (re-checked on every build)
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/(build|out|cmake-build-[^/]*|\\.vs|\\.idea)/")
add_executable(Engine ${ENGINE_SOURCES})
target_include_directories(Engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${stb_SOURCE_DIR})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_SOURCES})
target_link_libraries(Engine PRIVATE SDL3::SDL3-static imgui)

if (WIN32)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Engine)
    # Run from the source folder in VS so relative paths like "basalt.jpg" resolve next to main.cpp
    set_property(TARGET Engine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()
