#pragma once

// The shaders of platform/shaders, compiled into the program (Direct3D and
// Vulkan versions; src/CMakeLists.txt builds them).

#include <bgfx/bgfx.h>

// Program made of a vertex and a fragment shader of that name ("picture"
// gives picture_vs + picture_ps) for the renderer in use. Invalid handle if
// it does not exist
bgfx::ProgramHandle loadShaderProgram(const char* name);
