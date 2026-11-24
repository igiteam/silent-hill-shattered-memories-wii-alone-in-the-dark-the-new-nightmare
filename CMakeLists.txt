# Alone in the Dark: The New Nightmare: Classic Reborn
# Own reimplementation of the engine of Alone in the Dark: The New Nightmare
# (Darkworks, 2001). The game's data are never part of this project: every
# player uses their own copy.
#
# Visual Studio 2026:
#   cmake -G "Visual Studio 18 2026" -A x64 -S . -B build\vs2026
# then open build\vs2026\NewNightmare.sln (or .slnx) and build RelWithDebInfo.

cmake_minimum_required(VERSION 3.21)
project(NewNightmare C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    # The C++ runtime goes inside the executable (/MT): it runs on any Windows
    # without installing the Visual C++ Redistributable. Every library uses it.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    foreach(flags CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL
                  CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
        string(REPLACE "/MDd" "/MTd" ${flags} "${${flags}}")
        string(REPLACE "/MD" "/MT" ${flags} "${${flags}}")
    endforeach()
    # Several compilers of one project share its .pdb: /FS makes them take
    # turns (without it a parallel build fails with C1041)
    add_compile_options(/FS)
endif()

set(THIRD_PARTY "${CMAKE_SOURCE_DIR}/ThirdParty")

# bgfx: draws with Direct3D 11, Direct3D 12 or Vulkan. Only the renderer and
# its shader compiler (shaderc, used to build our shaders)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_BIN2C OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_GEOMETRY OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_TEXTURE OFF CACHE BOOL "" FORCE)
set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY}/bgfx.cmake)
# bgfx forces its example-common project while its tools are on; never built
if(TARGET example-common)
    set_target_properties(example-common PROPERTIES EXCLUDE_FROM_ALL ON EXCLUDE_FROM_DEFAULT_BUILD ON)
endif()

# SDL3: window, keyboard, mouse and controllers (static, without its tests)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
# Its version written here: otherwise SDL asks Git for it, and fails while
# this repository has no commit or is a copy without .git
set(SDL_REVISION "SDL3 (ThirdParty/SDL)" CACHE STRING "" FORCE)
add_subdirectory(${THIRD_PARTY}/SDL)

# zlib: most data files of the game are gzip. Only its static library
file(GLOB ZLIB_SOURCES "${THIRD_PARTY}/zlib/*.c")
configure_file("${THIRD_PARTY}/zlib/zconf.h.in" "${CMAKE_BINARY_DIR}/zlib/zconf.h" COPYONLY)
add_library(zlibstatic STATIC ${ZLIB_SOURCES})
target_include_directories(zlibstatic PUBLIC "${THIRD_PARTY}/zlib" "${CMAKE_BINARY_DIR}/zlib")
if(MSVC)
    target_compile_definitions(zlibstatic PRIVATE _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
endif()
set_target_properties(zlibstatic PROPERTIES FOLDER "ThirdParty")

# FFmpeg (LGPL, shared): plays the game's Bink videos. Not kept in Git:
# python tools/getFFmpeg.py puts it in ThirdParty/ffmpeg. Without it the
# program is built without videos
set(FFMPEG_DIR "${THIRD_PARTY}/ffmpeg")
if(EXISTS "${FFMPEG_DIR}/include/libavcodec/avcodec.h")
    add_library(ffmpeg INTERFACE)
    target_include_directories(ffmpeg INTERFACE "${FFMPEG_DIR}/include")
    foreach(lib avcodec avformat avutil swresample swscale)
        target_link_libraries(ffmpeg INTERFACE "${FFMPEG_DIR}/lib/${lib}.lib")
    endforeach()
    file(GLOB FFMPEG_DLLS "${FFMPEG_DIR}/bin/*.dll")
    set(NEWNIGHTMARE_VIDEO ON)
else()
    message(STATUS "FFmpeg not found in ThirdParty/ffmpeg: no videos (python tools/getFFmpeg.py)")
    set(NEWNIGHTMARE_VIDEO OFF)
endif()

add_subdirectory(src)

# F5 in Visual Studio starts our program
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NewNightmare)
