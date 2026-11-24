#include "embeddedShaders.h"

#include <cstring>

// Headers made by bgfx's shaderc while building (build folder, shaders/)
#include "dx11/picture_vs.sc.bin.h"
#include "dx11/picture_ps.sc.bin.h"
#include "spirv/picture_vs.sc.bin.h"
#include "spirv/picture_ps.sc.bin.h"
#include "dx11/foreground_vs.sc.bin.h"
#include "dx11/foreground_ps.sc.bin.h"
#include "spirv/foreground_vs.sc.bin.h"
#include "spirv/foreground_ps.sc.bin.h"
#include "dx11/model_vs.sc.bin.h"
#include "dx11/model_ps.sc.bin.h"
#include "spirv/model_vs.sc.bin.h"
#include "spirv/model_ps.sc.bin.h"
#include "dx11/overlay_vs.sc.bin.h"
#include "dx11/overlay_ps.sc.bin.h"
#include "spirv/overlay_vs.sc.bin.h"
#include "spirv/overlay_ps.sc.bin.h"
#include "dx11/beam_vs.sc.bin.h"
#include "dx11/beam_ps.sc.bin.h"
#include "spirv/beam_vs.sc.bin.h"
#include "spirv/beam_ps.sc.bin.h"
#include "dx11/blur_vs.sc.bin.h"
#include "dx11/blur_ps.sc.bin.h"
#include "spirv/blur_vs.sc.bin.h"
#include "spirv/blur_ps.sc.bin.h"

namespace {

struct ShaderCode {
    const uint8_t* data;
    uint32_t size;
};

struct ShaderProgram {
    const char* name;
    ShaderCode vertexDirect3D, fragmentDirect3D;
    ShaderCode vertexVulkan, fragmentVulkan;
};

#define SHADER_PROGRAM(name)                                                       \
    { #name,                                                                       \
      { name##_vs_dx11, sizeof(name##_vs_dx11) }, { name##_ps_dx11, sizeof(name##_ps_dx11) }, \
      { name##_vs_spv, sizeof(name##_vs_spv) }, { name##_ps_spv, sizeof(name##_ps_spv) } }

const ShaderProgram kPrograms[] = {
    SHADER_PROGRAM(picture),
    SHADER_PROGRAM(foreground),
    SHADER_PROGRAM(model),
    SHADER_PROGRAM(overlay),
    SHADER_PROGRAM(beam),
    SHADER_PROGRAM(blur),
};

bgfx::ShaderHandle createShader(const ShaderCode& code)
{
    return bgfx::createShader(bgfx::makeRef(code.data, code.size));
}

} // namespace

bgfx::ProgramHandle loadShaderProgram(const char* name)
{
    const bool vulkan = bgfx::getRendererType() == bgfx::RendererType::Vulkan;
    for (const ShaderProgram& program : kPrograms) {
        if (std::strcmp(program.name, name) != 0)
            continue;
        return bgfx::createProgram(createShader(vulkan ? program.vertexVulkan : program.vertexDirect3D),
                                   createShader(vulkan ? program.fragmentVulkan : program.fragmentDirect3D), true);
    }
    return BGFX_INVALID_HANDLE;
}
