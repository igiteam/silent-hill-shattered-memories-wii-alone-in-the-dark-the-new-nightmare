# The program, by layers (bottom to top): util -> platform -> core -> scripts
# (the game's script language) -> app.
# A layer never includes a header of a layer above it. Every .cpp and .h of
# these folders is compiled (re-run CMake after adding or removing a file).

set(LAYERS util platform core scripts app)

set(SOURCES "")
foreach(layer ${LAYERS})
    file(GLOB_RECURSE layer_files CONFIGURE_DEPENDS "${layer}/*.cpp" "${layer}/*.h")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${layer_files})
    list(APPEND SOURCES ${layer_files})
endforeach()

# Shaders: each .sc of platform/shaders is compiled by bgfx's shaderc into a
# header in the build folder (Direct3D and Vulkan versions), read by
# platform/embeddedShaders.cpp
set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/shaders")
set(SHADER_OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(SHADER_HEADERS "")
macro(add_shader_program vs ps varying)
    bgfx_compile_shaders(SHADERS "${SHADER_DIR}/${vs}.sc" TYPE VERTEX AS_HEADERS
        VARYING_DEF "${SHADER_DIR}/${varying}.sc" INCLUDE_DIRS "${BGFX_DIR}/src" "${SHADER_DIR}"
        OUTPUT_DIR "${SHADER_OUT}" OUT_FILES_VAR vs_out)
    bgfx_compile_shaders(SHADERS "${SHADER_DIR}/${ps}.sc" TYPE FRAGMENT AS_HEADERS
        VARYING_DEF "${SHADER_DIR}/${varying}.sc" INCLUDE_DIRS "${BGFX_DIR}/src" "${SHADER_DIR}"
        OUTPUT_DIR "${SHADER_OUT}" OUT_FILES_VAR ps_out)
    list(APPEND SHADER_HEADERS ${vs_out} ${ps_out})
    list(APPEND SOURCES "${SHADER_DIR}/${vs}.sc" "${SHADER_DIR}/${ps}.sc" "${SHADER_DIR}/${varying}.sc")
endmacro()

add_shader_program(picture_vs picture_ps picture.varying.def)
add_shader_program(foreground_vs foreground_ps scene.varying.def)
add_shader_program(model_vs model_ps scene.varying.def)
add_shader_program(overlay_vs overlay_ps overlay.varying.def)
add_shader_program(beam_vs beam_ps scene.varying.def)
add_shader_program(blur_vs blur_ps picture.varying.def)
# Pieces shared by several shaders
file(GLOB SHADER_INCLUDES CONFIGURE_DEPENDS "${SHADER_DIR}/*.sh")
list(APPEND SOURCES ${SHADER_INCLUDES})

source_group("platform/shaders" FILES ${SHADER_HEADERS})

add_executable(NewNightmare ${SOURCES} ${SHADER_HEADERS})
target_include_directories(NewNightmare PRIVATE ${LAYERS} "${SHADER_OUT}")
target_link_libraries(NewNightmare PRIVATE bgfx bx SDL3-static zlibstatic)
if(NEWNIGHTMARE_VIDEO)
    target_link_libraries(NewNightmare PRIVATE ffmpeg)
    target_compile_definitions(NewNightmare PRIVATE NEWNIGHTMARE_VIDEO=1)
    # Its DLLs next to the program
    add_custom_command(TARGET NewNightmare POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FFMPEG_DLLS} "$<TARGET_FILE_DIR:NewNightmare>")
endif()
if(MSVC)
    target_compile_options(NewNightmare PRIVATE /W4 /Zc:__cplusplus /Zc:preprocessor /utf-8)
    target_compile_definitions(NewNightmare PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(NewNightmare PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:NewNightmare>"
    # No console window except in Debug builds
    WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)

# Folder of the game given to the program when Visual Studio starts it (F5):
#   cmake ... -DNEWNIGHTMARE_DATA="C:/Games/Alone in the Dark - The New Nightmare"
set(NEWNIGHTMARE_DATA "" CACHE PATH "Game data folder passed with --data when started from Visual Studio")
if(NEWNIGHTMARE_DATA)
    set_target_properties(NewNightmare PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "--data \"${NEWNIGHTMARE_DATA}\"")
endif()
