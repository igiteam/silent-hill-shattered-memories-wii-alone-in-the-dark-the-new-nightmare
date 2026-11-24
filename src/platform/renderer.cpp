#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include "embeddedShaders.h"
#include "window.h"

namespace {

struct PictureVertex {
    float x, y, z;
    float u, v;
};

struct OverlayVertex {
    float x, y, z;
    float u, v;
    uint32_t colour;
};

// The two versions of a texture: in the dark and lit
enum PictureKind { Dark, Lit, PictureKinds };

// A pair of textures (dark and lit), kept here too: after a change of
// renderer they are uploaded again
struct TexturePair {
    std::vector<uint32_t> colours[PictureKinds];
    int width = 0, height = 0;
    bool changed = false;
    bgfx::TextureHandle handles[PictureKinds] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    int handleWidth = 0, handleHeight = 0;

    void set(int w, int h, const uint32_t* dark, const uint32_t* lit)
    {
        const size_t count = size_t(w) * h;
        colours[Dark].assign(dark, dark + count);
        colours[Lit].assign(lit, lit + count);
        width = w;
        height = h;
        changed = true;
    }

    void destroy()
    {
        for (bgfx::TextureHandle& handle : handles) {
            if (bgfx::isValid(handle))
                bgfx::destroy(handle);
            handle = BGFX_INVALID_HANDLE;
        }
        handleWidth = handleHeight = 0;
    }

    void upload()
    {
        if (!changed || colours[Dark].empty())
            return;
        if (handleWidth != width || handleHeight != height)
            destroy();
        for (int kind = 0; kind < PictureKinds; kind++) {
            if (!bgfx::isValid(handles[kind]))
                handles[kind] = bgfx::createTexture2D(uint16_t(width), uint16_t(height), false, 1,
                                                      bgfx::TextureFormat::RGBA8,
                                                      BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
            bgfx::updateTexture2D(handles[kind], 0, 0, 0, 0, uint16_t(width), uint16_t(height),
                                  bgfx::copy(colours[kind].data(), uint32_t(colours[kind].size() * sizeof(uint32_t))));
        }
        handleWidth = width;
        handleHeight = height;
        changed = false;
    }

    bool ready() const { return bgfx::isValid(handles[Dark]) && bgfx::isValid(handles[Lit]); }
};

// Depth of the 3D meshes: what is nearer or farther from the camera is not drawn
constexpr float kNear = 10.0f;
constexpr float kFar = 100000.0f;

bgfx::VertexLayout s_textureLayout;  // position + texture place (pictures and meshes)
bgfx::VertexLayout s_modelLayout;    // position + normal + texture place (models)
bgfx::VertexLayout s_overlayLayout;  // position + texture place + colour (the layer over everything)
bgfx::ProgramHandle s_pictureProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle s_foregroundProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle s_modelProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle s_overlayProgram = BGFX_INVALID_HANDLE;
bgfx::UniformHandle s_overlaySampler = BGFX_INVALID_HANDLE;
bgfx::UniformHandle s_modelSampler = BGFX_INVALID_HANDLE;
bgfx::UniformHandle s_samplers[PictureKinds] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
bgfx::UniformHandle s_lightUniform = BGFX_INVALID_HANDLE;
bgfx::UniformHandle s_spotPlaceUniform = BGFX_INVALID_HANDLE;
bgfx::UniformHandle s_spotAimUniform = BGFX_INVALID_HANDLE;
bgfx::UniformHandle s_cameraUniform = BGFX_INVALID_HANDLE;
bgfx::UniformHandle s_frameUniform = BGFX_INVALID_HANDLE;
bgfx::UniformHandle s_highlightUniform = BGFX_INVALID_HANDLE;
int s_width = 0, s_height = 0;
bool s_started = false;

TexturePair s_picture;
// Mask of the foreground: one byte a pixel, kept for a change of renderer
std::vector<uint8_t> s_mask;
int s_maskWidth = 0, s_maskHeight = 0;
bool s_maskChanged = false;
bgfx::TextureHandle s_maskTexture = BGFX_INVALID_HANDLE;
bgfx::UniformHandle s_maskSampler = BGFX_INVALID_HANDLE;
std::vector<MeshVertex> s_foregroundPoints;
std::vector<uint32_t> s_foregroundTriangles;
// The same split: the room's and the objects' (only where the mask says)
std::vector<uint32_t> s_roomTriangles, s_objectTriangles;
std::vector<ModelPoint> s_modelPoints;
std::vector<ModelPart> s_modelParts;
// Textures of the model (kept for a change of renderer) and a grey one for
// the parts without texture
std::vector<ModelImage> s_modelImages;
std::vector<bgfx::TextureHandle> s_modelTextures;
bool s_modelTexturesChanged = false;
bgfx::TextureHandle s_greyTexture = BGFX_INVALID_HANDLE;
float s_focal = 0.0f;
bool s_highlight = false;
// The layer over everything: its image (kept for a change of renderer), a
// white one for the plain rectangles, and what it shows
std::vector<uint32_t> s_overlayPixels;
int s_overlayWidth = 0, s_overlayHeight = 0;
bool s_overlayChanged = false;
bool s_overlaySmooth = false;  // its image blends the nearest pixels
bgfx::TextureHandle s_overlayTexture = BGFX_INVALID_HANDLE;
bgfx::TextureHandle s_whiteTexture = BGFX_INVALID_HANDLE;
std::vector<OverlayRect> s_overlayRects;
// The glows: their image and rectangles, added as light
std::vector<uint32_t> s_glowPixels;
int s_glowWidth = 0, s_glowHeight = 0;
bool s_glowChanged = false;
bgfx::TextureHandle s_glowTexture = BGFX_INVALID_HANDLE;
std::vector<OverlayRect> s_glowRects;
std::vector<GlowQuad> s_glowQuads;
float s_bandHeight = 0;  // the cinema bands
// The models' light and the shadows
float s_ambient[4] = { 0.16f, 0.16f, 0.16f, 0 }, s_sceneLight[4] = {};
bgfx::UniformHandle s_ambientUniform = BGFX_INVALID_HANDLE, s_sceneLightUniform = BGFX_INVALID_HANDLE;
std::vector<float> s_shadowTriangles;
float s_shadowDarkness = 0;

// Light everywhere, and the flashlight (shaders/flashlight.sh)
float s_lightEverywhere = 1.0f;
float s_spotPlace[4] = {}, s_spotAim[4] = { 0, 1, 0, 625 };

// The views: the image of the beam (the scene's mesh lit, with its own
// depth), blurred across and down; the picture lit by it; then the models,
// the foreground and the layer over everything
constexpr bgfx::ViewId kBeamView = 0, kBlurAcrossView = 1, kBlurDownView = 2, kPictureView = 3, kModelView = 4;

// The image of the beam: 640 x 480 like the pictures; A (with depth) is drawn
// in and blurred again in, B holds the blur across. Black when off
constexpr uint16_t kBeamWidth = 640, kBeamHeight = 480;
bgfx::FrameBufferHandle s_beamA = BGFX_INVALID_HANDLE, s_beamB = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle s_beamProgram = BGFX_INVALID_HANDLE, s_blurProgram = BGFX_INVALID_HANDLE;
bgfx::UniformHandle s_beamSampler = BGFX_INVALID_HANDLE, s_blurUniform = BGFX_INVALID_HANDLE;
bgfx::TextureHandle s_blackTexture = BGFX_INVALID_HANDLE;

void destroyMaskTexture()
{
    if (bgfx::isValid(s_maskTexture))
        bgfx::destroy(s_maskTexture);
    s_maskTexture = BGFX_INVALID_HANDLE;
}

// Uploads the mask, read pixel by pixel (no smoothing: its pieces lie side by
// side on the sheet and must not mix)
void uploadMask()
{
    if (!s_maskChanged)
        return;
    destroyMaskTexture();
    if (!s_mask.empty())
        s_maskTexture = bgfx::createTexture2D(uint16_t(s_maskWidth), uint16_t(s_maskHeight), false, 1,
                                              bgfx::TextureFormat::R8,
                                              BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_POINT,
                                              bgfx::copy(s_mask.data(), uint32_t(s_mask.size())));
    s_maskChanged = false;
}

void destroyModelTextures()
{
    for (bgfx::TextureHandle& texture : s_modelTextures)
        if (bgfx::isValid(texture))
            bgfx::destroy(texture);
    s_modelTextures.clear();
}

void uploadModelTextures()
{
    if (!s_modelTexturesChanged)
        return;
    destroyModelTextures();
    for (const ModelImage& image : s_modelImages)
        s_modelTextures.push_back(bgfx::createTexture2D(
            uint16_t(image.size), uint16_t(image.size), false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(image.rgba.data(), uint32_t(image.rgba.size() * sizeof(uint32_t)))));
    s_modelTexturesChanged = false;
}

bgfx::RendererType::Enum toBgfx(RendererChoice choice)
{
    switch (choice) {
    case RendererChoice::Direct3D12: return bgfx::RendererType::Direct3D12;
    case RendererChoice::Vulkan: return bgfx::RendererType::Vulkan;
    case RendererChoice::Direct3D11:
    case RendererChoice::Auto:
    default: return bgfx::RendererType::Direct3D11;
    }
}

// Size of the 4:3 picture in the middle of the window, as a fraction of the
// window (1 on the side that fills it)
void pictureScale(int windowWidth, int windowHeight, float& sx, float& sy)
{
    sx = sy = 1.0f;
    const float window = float(windowWidth) / float(windowHeight);
    if (window > 4.0f / 3.0f)
        sx = (4.0f / 3.0f) / window;
    else
        sy = window / (4.0f / 3.0f);
}

void destroyHandle(bgfx::UniformHandle& handle)
{
    if (bgfx::isValid(handle))
        bgfx::destroy(handle);
    handle = BGFX_INVALID_HANDLE;
}

void destroyHandle(bgfx::ProgramHandle& handle)
{
    if (bgfx::isValid(handle))
        bgfx::destroy(handle);
    handle = BGFX_INVALID_HANDLE;
}

// A transient copy of those points for this frame. False if there is no room
template <typename Vertex>
bool copyPoints(const std::vector<Vertex>& points, const bgfx::VertexLayout& layout,
                bgfx::TransientVertexBuffer& out)
{
    const uint32_t count = uint32_t(points.size());
    if (count == 0 || bgfx::getAvailTransientVertexBuffer(count, layout) < count)
        return false;
    bgfx::allocTransientVertexBuffer(&out, count, layout);
    std::memcpy(out.data, points.data(), count * sizeof(Vertex));
    return true;
}

// Sets those points and triangles for the next draw, if there is room
bool setTriangles(const bgfx::TransientVertexBuffer& points, const std::vector<uint32_t>& triangles)
{
    const uint32_t count = uint32_t(triangles.size());
    if (count == 0 || bgfx::getAvailTransientIndexBuffer(count, true) < count)
        return false;
    bgfx::TransientIndexBuffer indices;
    bgfx::allocTransientIndexBuffer(&indices, count, true);
    std::memcpy(indices.data, triangles.data(), count * sizeof(uint32_t));
    bgfx::setVertexBuffer(0, &points);
    bgfx::setIndexBuffer(&indices);
    return true;
}

void destroyOverlayTexture()
{
    if (bgfx::isValid(s_overlayTexture))
        bgfx::destroy(s_overlayTexture);
    s_overlayTexture = BGFX_INVALID_HANDLE;
}

// Uploads the layer's image, read pixel by pixel (sharp letters) or smooth
void uploadOverlayImage()
{
    if (!s_overlayChanged)
        return;
    destroyOverlayTexture();
    if (!s_overlayPixels.empty())
        s_overlayTexture = bgfx::createTexture2D(
            uint16_t(s_overlayWidth), uint16_t(s_overlayHeight), false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | (s_overlaySmooth ? 0 : BGFX_SAMPLER_POINT),
            bgfx::copy(s_overlayPixels.data(), uint32_t(s_overlayPixels.size() * sizeof(uint32_t))));
    s_overlayChanged = false;
}

// Draws the rectangles that are (or are not) textured, with an image (else
// plain white) and a way of blending, over the picture placed at sx, sy of the
// window (as pictureScale)
void drawRects(const std::vector<OverlayRect>& rects, bool textured, bgfx::TextureHandle texture, uint64_t blend,
               float sx, float sy)
{
    std::vector<OverlayVertex> points;
    for (const OverlayRect& r : rects) {
        if (r.textured != textured)
            continue;
        // The corners in pixels of the picture, turned about the middle of
        // the left side, then in the window
        const float pivotX = r.x, pivotY = r.y + r.height * 0.5f;
        const float cs = std::cos(r.angle), sn = std::sin(r.angle);
        auto corner = [&](float px, float py, float u, float v) {
            const float dx = px - pivotX, dy = py - pivotY;
            const float x = pivotX + dx * cs - dy * sn, y = pivotY + dx * sn + dy * cs;
            return OverlayVertex{ -sx + 2.0f * sx * x / 640.0f, sy - 2.0f * sy * y / 480.0f, 0, u, v, r.colour };
        };
        const OverlayVertex a = corner(r.x, r.y, r.u0, r.v0), b = corner(r.x + r.width, r.y, r.u1, r.v0);
        const OverlayVertex c = corner(r.x, r.y + r.height, r.u0, r.v1);
        const OverlayVertex d = corner(r.x + r.width, r.y + r.height, r.u1, r.v1);
        points.insert(points.end(), { a, b, c, c, b, d });
    }
    bgfx::TextureHandle image = textured ? texture : s_whiteTexture;
    bgfx::TransientVertexBuffer buffer;
    if (points.empty() || !bgfx::isValid(image) || !copyPoints(points, s_overlayLayout, buffer))
        return;
    bgfx::setVertexBuffer(0, &buffer);
    bgfx::setTexture(0, s_overlaySampler, image);
    bgfx::setState(BGFX_STATE_WRITE_RGB | blend);
    bgfx::submit(kModelView, s_overlayProgram);
}

void drawOverlay(bool textured, float sx, float sy)
{
    drawRects(s_overlayRects, textured, s_overlayTexture, BGFX_STATE_BLEND_ALPHA, sx, sy);
}

void uploadGlowImage()
{
    if (!s_glowChanged)
        return;
    if (bgfx::isValid(s_glowTexture))
        bgfx::destroy(s_glowTexture);
    s_glowTexture = BGFX_INVALID_HANDLE;
    if (!s_glowPixels.empty())
        s_glowTexture = bgfx::createTexture2D(
            uint16_t(s_glowWidth), uint16_t(s_glowHeight), false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(s_glowPixels.data(), uint32_t(s_glowPixels.size() * sizeof(uint32_t))));
    s_glowChanged = false;
}

} // namespace

RendererChoice parseRendererChoice(const char* name)
{
    if (!name)
        return RendererChoice::Auto;
    if (std::strcmp(name, "d3d11") == 0)
        return RendererChoice::Direct3D11;
    if (std::strcmp(name, "d3d12") == 0)
        return RendererChoice::Direct3D12;
    if (std::strcmp(name, "vulkan") == 0)
        return RendererChoice::Vulkan;
    return RendererChoice::Auto;
}

RendererChoice nextRendererChoice(RendererChoice choice)
{
    switch (choice) {
    case RendererChoice::Auto:
    case RendererChoice::Direct3D11: return RendererChoice::Direct3D12;
    case RendererChoice::Direct3D12: return RendererChoice::Vulkan;
    default: return RendererChoice::Direct3D11;
    }
}

bool initRenderer(RendererChoice choice, int width, int height)
{
    // Calling renderFrame before init keeps bgfx on this thread (no render thread)
    bgfx::renderFrame();
    bgfx::Init init;
    init.type = toBgfx(choice);
    init.platformData.nwh = nativeWindowHandle();
    init.resolution.width = uint32_t(width);
    init.resolution.height = uint32_t(height);
    init.resolution.reset = BGFX_RESET_VSYNC;
    if (!bgfx::init(init))
        return false;
    s_started = true;
    s_width = width;
    s_height = height;
    bgfx::setViewClear(kPictureView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000FF, 1.0f, 0);
    bgfx::setViewClear(kModelView, BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, 0x000000FF, 1.0f, 0);
    // In the order they are given: picture, beam; model, foreground, layer
    bgfx::setViewMode(kPictureView, bgfx::ViewMode::Sequential);
    bgfx::setViewMode(kModelView, bgfx::ViewMode::Sequential);
    s_textureLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    s_modelLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    s_pictureProgram = loadShaderProgram("picture");
    s_foregroundProgram = loadShaderProgram("foreground");
    s_modelProgram = loadShaderProgram("model");
    s_overlayProgram = loadShaderProgram("overlay");
    s_overlayLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
    s_overlaySampler = bgfx::createUniform("s_overlay", bgfx::UniformType::Sampler);
    const uint32_t white = 0xFFFFFFFFu;
    s_whiteTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::copy(&white, 4));
    s_overlayChanged = true;
    s_modelSampler = bgfx::createUniform("s_texture", bgfx::UniformType::Sampler);
    const uint32_t grey = 0xFFB0B0B0u;
    s_greyTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::copy(&grey, 4));
    s_modelTexturesChanged = true;
    s_samplers[Dark] = bgfx::createUniform("s_dark", bgfx::UniformType::Sampler);
    s_samplers[Lit] = bgfx::createUniform("s_lit", bgfx::UniformType::Sampler);
    s_maskSampler = bgfx::createUniform("s_mask", bgfx::UniformType::Sampler);
    s_lightUniform = bgfx::createUniform("u_light", bgfx::UniformType::Vec4);
    s_spotPlaceUniform = bgfx::createUniform("u_spotPlace", bgfx::UniformType::Vec4);
    s_spotAimUniform = bgfx::createUniform("u_spotAim", bgfx::UniformType::Vec4);
    s_cameraUniform = bgfx::createUniform("u_camera", bgfx::UniformType::Vec4);
    s_frameUniform = bgfx::createUniform("u_frame", bgfx::UniformType::Vec4);
    s_highlightUniform = bgfx::createUniform("u_highlight", bgfx::UniformType::Vec4);
    s_ambientUniform = bgfx::createUniform("u_ambient", bgfx::UniformType::Vec4);
    s_sceneLightUniform = bgfx::createUniform("u_sceneLight", bgfx::UniformType::Vec4);
    s_beamProgram = loadShaderProgram("beam");
    s_blurProgram = loadShaderProgram("blur");
    s_beamSampler = bgfx::createUniform("s_beam", bgfx::UniformType::Sampler);
    s_blurUniform = bgfx::createUniform("u_blur", bgfx::UniformType::Vec4);
    const uint32_t black = 0xFF000000u;
    s_blackTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::copy(&black, 4));
    {
        const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        bgfx::TextureHandle a[2] = {
            bgfx::createTexture2D(kBeamWidth, kBeamHeight, false, 1, bgfx::TextureFormat::RGBA8, flags),
            bgfx::createTexture2D(kBeamWidth, kBeamHeight, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY)
        };
        s_beamA = bgfx::createFrameBuffer(2, a, true);
        s_beamB = bgfx::createFrameBuffer(kBeamWidth, kBeamHeight, bgfx::TextureFormat::RGBA8, flags);
    }
    bgfx::setViewClear(kBeamView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000FF, 1.0f, 0);
    bgfx::setViewMode(kBeamView, bgfx::ViewMode::Sequential);
    s_picture.changed = s_maskChanged = true;
    return bgfx::isValid(s_pictureProgram) && bgfx::isValid(s_foregroundProgram) && bgfx::isValid(s_modelProgram)
           && bgfx::isValid(s_overlayProgram) && bgfx::isValid(s_beamProgram) && bgfx::isValid(s_blurProgram);
}

void shutdownRenderer()
{
    if (!s_started)
        return;
    s_picture.destroy();
    destroyMaskTexture();
    destroyHandle(s_maskSampler);
    for (bgfx::UniformHandle& sampler : s_samplers)
        destroyHandle(sampler);
    destroyHandle(s_lightUniform);
    destroyHandle(s_spotPlaceUniform);
    destroyHandle(s_spotAimUniform);
    destroyHandle(s_cameraUniform);
    destroyHandle(s_frameUniform);
    destroyHandle(s_highlightUniform);
    destroyHandle(s_ambientUniform);
    destroyHandle(s_sceneLightUniform);
    destroyHandle(s_beamProgram);
    destroyHandle(s_blurProgram);
    destroyHandle(s_beamSampler);
    destroyHandle(s_blurUniform);
    if (bgfx::isValid(s_blackTexture))
        bgfx::destroy(s_blackTexture);
    s_blackTexture = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(s_beamA))
        bgfx::destroy(s_beamA);
    if (bgfx::isValid(s_beamB))
        bgfx::destroy(s_beamB);
    s_beamA = s_beamB = BGFX_INVALID_HANDLE;
    destroyHandle(s_pictureProgram);
    destroyHandle(s_foregroundProgram);
    destroyHandle(s_modelProgram);
    destroyHandle(s_modelSampler);
    destroyHandle(s_overlayProgram);
    destroyHandle(s_overlaySampler);
    destroyOverlayTexture();
    if (bgfx::isValid(s_glowTexture))
        bgfx::destroy(s_glowTexture);
    s_glowTexture = BGFX_INVALID_HANDLE;
    s_glowChanged = true;
    if (bgfx::isValid(s_whiteTexture))
        bgfx::destroy(s_whiteTexture);
    s_whiteTexture = BGFX_INVALID_HANDLE;
    destroyModelTextures();
    if (bgfx::isValid(s_greyTexture))
        bgfx::destroy(s_greyTexture);
    s_greyTexture = BGFX_INVALID_HANDLE;
    bgfx::shutdown();
    s_started = false;
}

const char* rendererName()
{
    return s_started ? bgfx::getRendererName(bgfx::getRendererType()) : "none";
}

void setPictures(int width, int height, const uint32_t* dark, const uint32_t* lit)
{
    s_picture.set(width, height, dark, lit);
}

void setLight(float everywhere)
{
    s_lightEverywhere = everywhere;
}

void setFlashlight(bool on, const float place[3], const float aim[3], float reach)
{
    for (int k = 0; k < 3; k++) {
        s_spotPlace[k] = place[k];
        s_spotAim[k] = aim[k];
    }
    s_spotPlace[3] = on ? 1.0f : 0.0f;
    s_spotAim[3] = reach;
}

void setCameraFocal(float focal)
{
    s_focal = focal;
}

void setForegroundMesh(const std::vector<MeshVertex>& points, const std::vector<uint32_t>& triangles,
                       size_t roomCount, int width, int height, const uint8_t* mask)
{
    s_foregroundPoints = points;
    s_foregroundTriangles = triangles;
    s_roomTriangles.assign(triangles.begin(), triangles.begin() + std::min(roomCount, triangles.size()));
    s_objectTriangles.assign(triangles.begin() + std::min(roomCount, triangles.size()), triangles.end());
    s_mask.assign(mask, mask + size_t(width) * height);
    s_maskWidth = width;
    s_maskHeight = height;
    s_maskChanged = true;
}

void setModelTextures(const std::vector<ModelImage>& textures)
{
    s_modelImages = textures;
    s_modelTexturesChanged = true;
}

void setModel(const std::vector<ModelPoint>& points, const std::vector<ModelPart>& parts)
{
    s_modelPoints = points;
    s_modelParts = parts;
}

void setOverlayImage(int width, int height, const uint32_t* rgba, bool smooth)
{
    s_overlaySmooth = smooth;
    s_overlayPixels.assign(rgba, rgba + size_t(width) * height);
    s_overlayWidth = width;
    s_overlayHeight = height;
    s_overlayChanged = true;
}

void setGlowImage(int width, int height, const uint32_t* rgba)
{
    s_glowPixels.assign(rgba, rgba + size_t(width) * size_t(height));
    s_glowWidth = width;
    s_glowHeight = height;
    s_glowChanged = true;
}

void setModelLight(const float ambient[3], const float lightPlace[3], float lightStrength)
{
    for (int k = 0; k < 3; k++) {
        s_ambient[k] = ambient[k];
        s_sceneLight[k] = lightPlace[k];
    }
    s_sceneLight[3] = lightStrength;
}

void setShadows(const std::vector<float>& triangles, float darkness)
{
    s_shadowTriangles = triangles;
    s_shadowDarkness = darkness;
}

void setCinemaBands(float height)
{
    s_bandHeight = height;
}

void setGlowQuads(const std::vector<GlowQuad>& quads)
{
    s_glowQuads = quads;
}

void setGlows(const std::vector<OverlayRect>& rects)
{
    s_glowRects = rects;
}

void setOverlay(const std::vector<OverlayRect>& rects)
{
    s_overlayRects = rects;
}

void setForegroundHighlight(bool on)
{
    s_highlight = on;
}

bool pictureFromWindow(float windowX, float windowY, float& x, float& y)
{
    if (s_width <= 0 || s_height <= 0)
        return false;
    float sx, sy;
    pictureScale(s_width, s_height, sx, sy);
    x = (windowX - (1.0f - sx) * 0.5f * float(s_width)) / (sx * float(s_width));
    y = (windowY - (1.0f - sy) * 0.5f * float(s_height)) / (sy * float(s_height));
    return x >= 0.0f && x <= 1.0f && y >= 0.0f && y <= 1.0f;
}

// Half of the blur of the beam's image: from one image into a frame buffer,
// a step of a texel along (dx, dy)
void blurBeam(bgfx::ViewId view, bgfx::FrameBufferHandle into, bgfx::TextureHandle from, float dx, float dy)
{
    if (bgfx::getAvailTransientVertexBuffer(6, s_textureLayout) != 6)
        return;
    bgfx::setViewFrameBuffer(view, into);
    bgfx::setViewRect(view, 0, 0, kBeamWidth, kBeamHeight);
    bgfx::TransientVertexBuffer buffer;
    bgfx::allocTransientVertexBuffer(&buffer, 6, s_textureLayout);
    PictureVertex* v = reinterpret_cast<PictureVertex*>(buffer.data);
    const PictureVertex topLeft = { -1, 1, 0, 0, 0 }, topRight = { 1, 1, 0, 1, 0 };
    const PictureVertex bottomLeft = { -1, -1, 0, 0, 1 }, bottomRight = { 1, -1, 0, 1, 1 };
    v[0] = topLeft; v[1] = topRight; v[2] = bottomLeft;
    v[3] = bottomLeft; v[4] = topRight; v[5] = bottomRight;
    const float step[4] = { dx / float(kBeamWidth), dy / float(kBeamHeight), 0, 0 };
    bgfx::setVertexBuffer(0, &buffer);
    bgfx::setTexture(0, s_beamSampler, from);
    bgfx::setUniform(s_blurUniform, step);
    bgfx::setState(BGFX_STATE_WRITE_RGB);
    bgfx::submit(view, s_blurProgram);
}

void drawFrame(int windowWidth, int windowHeight)
{
    if (!s_started || windowWidth <= 0 || windowHeight <= 0)
        return;
    if (windowWidth != s_width || windowHeight != s_height) {
        bgfx::reset(uint32_t(windowWidth), uint32_t(windowHeight), BGFX_RESET_VSYNC);
        s_width = windowWidth;
        s_height = windowHeight;
    }
    s_picture.upload();
    uploadMask();
    uploadModelTextures();
    uploadOverlayImage();
    uploadGlowImage();

    bgfx::setViewRect(kPictureView, 0, 0, uint16_t(windowWidth), uint16_t(windowHeight));
    bgfx::setViewRect(kModelView, 0, 0, uint16_t(windowWidth), uint16_t(windowHeight));
    bgfx::touch(kPictureView);
    bgfx::touch(kModelView);

    float sx, sy;
    pictureScale(windowWidth, windowHeight, sx, sy);
    const float left = (1.0f - sx) * 0.5f * float(windowWidth);
    const float top = (1.0f - sy) * 0.5f * float(windowHeight);
    const float pictureWidth = sx * float(windowWidth), pictureHeight = sy * float(windowHeight);
    // For the shaders: the light everywhere (w)
    const float light[4] = { 0, 0, 0, s_lightEverywhere };
    // Where the picture is in the window, in pixels
    const float frame[4] = { left, top, pictureWidth, pictureHeight };
    // Camera space to the window: x and z over the depth y, as the game projects
    const float camera[4] = { s_picture.width > 0 ? s_focal / (0.5f * float(s_picture.width)) * sx : 0.0f,
                              s_picture.height > 0 ? s_focal / (0.5f * float(s_picture.height)) * sy : 0.0f,
                              kFar / (kFar - kNear), -kNear * kFar / (kFar - kNear) };
    const float highlight[4] = { s_highlight ? 1.0f : 0.0f, 0, 0, 0 };

    // The image of the flashlight's beam (ours): the scene's mesh lit at each
    // of its points (the nearest one at each pixel; the objects of the
    // foreground only where the mask says), then blurred across and down, so
    // that the light fades softly instead of ending at the edges of the
    // foreground's pieces
    bgfx::TextureHandle beam = s_blackTexture;
    bgfx::TransientVertexBuffer meshPoints;
    if (s_spotPlace[3] > 0.0f && s_focal > 0.0f && s_picture.ready() && bgfx::isValid(s_maskTexture)
        && bgfx::isValid(s_beamA) && bgfx::isValid(s_beamB)
        && copyPoints(s_foregroundPoints, s_textureLayout, meshPoints)) {
        bgfx::setViewFrameBuffer(kBeamView, s_beamA);
        bgfx::setViewRect(kBeamView, 0, 0, kBeamWidth, kBeamHeight);
        bgfx::touch(kBeamView);
        const float wholeCamera[4] = { s_focal / (0.5f * float(kBeamWidth)), s_focal / (0.5f * float(kBeamHeight)),
                                       kFar / (kFar - kNear), -kNear * kFar / (kFar - kNear) };
        const float roomEverywhere[4] = { 0, 1, 0, 0 }, objectsOnly[4] = { 0, 1, 1, 0 };
        for (int part = 0; part < 2; part++) {
            if (!setTriangles(meshPoints, part == 0 ? s_roomTriangles : s_objectTriangles))
                continue;
            bgfx::setTexture(2, s_maskSampler, s_maskTexture);
            bgfx::setUniform(s_cameraUniform, wholeCamera);
            bgfx::setUniform(s_lightUniform, light);
            bgfx::setUniform(s_spotPlaceUniform, s_spotPlace);
            bgfx::setUniform(s_spotAimUniform, s_spotAim);
            bgfx::setUniform(s_highlightUniform, part == 0 ? roomEverywhere : objectsOnly);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
            bgfx::submit(kBeamView, s_beamProgram);
        }
        blurBeam(kBlurAcrossView, s_beamB, bgfx::getTexture(s_beamA, 0), 1.5f, 0.0f);
        blurBeam(kBlurDownView, s_beamA, bgfx::getTexture(s_beamB, 0), 0.0f, 1.5f);
        beam = bgfx::getTexture(s_beamA, 0);
    }

    if (s_picture.ready() && bgfx::getAvailTransientVertexBuffer(6, s_textureLayout) == 6) {
        bgfx::TransientVertexBuffer buffer;
        bgfx::allocTransientVertexBuffer(&buffer, 6, s_textureLayout);
        PictureVertex* v = reinterpret_cast<PictureVertex*>(buffer.data);
        const PictureVertex topLeft = { -sx, sy, 0, 0, 0 }, topRight = { sx, sy, 0, 1, 0 };
        const PictureVertex bottomLeft = { -sx, -sy, 0, 0, 1 }, bottomRight = { sx, -sy, 0, 1, 1 };
        v[0] = topLeft; v[1] = topRight; v[2] = bottomLeft;
        v[3] = bottomLeft; v[4] = topRight; v[5] = bottomRight;
        bgfx::setVertexBuffer(0, &buffer);
        bgfx::setTexture(0, s_samplers[Dark], s_picture.handles[Dark]);
        bgfx::setTexture(1, s_samplers[Lit], s_picture.handles[Lit]);
        bgfx::setTexture(2, s_beamSampler, beam);
        bgfx::setUniform(s_lightUniform, light);
        bgfx::setState(BGFX_STATE_WRITE_RGB);
        bgfx::submit(kPictureView, s_pictureProgram);
    }

    // The shadows on the floor, each pixel darkened once (the stencil counts)
    if (!s_shadowTriangles.empty() && s_shadowDarkness > 0.0f) {
        std::vector<OverlayVertex> points;
        const uint32_t colour = uint32_t(std::clamp(s_shadowDarkness, 0.0f, 1.0f) * 255.0f) << 24;
        for (size_t k = 0; k + 1 < s_shadowTriangles.size(); k += 2)
            points.push_back({ -sx + 2.0f * sx * s_shadowTriangles[k] / 640.0f, sy - 2.0f * sy * s_shadowTriangles[k + 1] / 480.0f,
                               0, 0, 0, colour });
        bgfx::TransientVertexBuffer buffer;
        if (copyPoints(points, s_overlayLayout, buffer)) {
            bgfx::setVertexBuffer(0, &buffer);
            bgfx::setTexture(0, s_overlaySampler, s_whiteTexture);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
            bgfx::setStencil(BGFX_STENCIL_TEST_EQUAL | BGFX_STENCIL_FUNC_REF(0) | BGFX_STENCIL_FUNC_RMASK(0xFF)
                             | BGFX_STENCIL_OP_FAIL_S_KEEP | BGFX_STENCIL_OP_FAIL_Z_KEEP | BGFX_STENCIL_OP_PASS_Z_INCR);
            bgfx::submit(kModelView, s_overlayProgram);
        }
    }

    bgfx::TransientVertexBuffer modelPoints;
    if (s_focal > 0.0f && copyPoints(s_modelPoints, s_modelLayout, modelPoints)) {
        for (const ModelPart& part : s_modelParts) {
            if (!setTriangles(modelPoints, part.triangles))
                continue;
            const bool textured = part.texture >= 0 && size_t(part.texture) < s_modelTextures.size();
            bgfx::setTexture(0, s_modelSampler, textured ? s_modelTextures[size_t(part.texture)] : s_greyTexture);
            bgfx::setUniform(s_cameraUniform, camera);
            bgfx::setUniform(s_lightUniform, light);
            bgfx::setUniform(s_spotPlaceUniform, s_spotPlace);
            bgfx::setUniform(s_spotAimUniform, s_spotAim);
            bgfx::setUniform(s_ambientUniform, s_ambient);
            bgfx::setUniform(s_sceneLightUniform, s_sceneLight);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
            bgfx::submit(kModelView, s_modelProgram);
        }
    }

    bgfx::TransientVertexBuffer foregroundPoints;
    if (s_focal > 0.0f && s_picture.ready() && bgfx::isValid(s_maskTexture)
        && copyPoints(s_foregroundPoints, s_textureLayout, foregroundPoints)
        && setTriangles(foregroundPoints, s_foregroundTriangles)) {
        bgfx::setTexture(0, s_samplers[Dark], s_picture.handles[Dark]);
        bgfx::setTexture(1, s_samplers[Lit], s_picture.handles[Lit]);
        bgfx::setTexture(2, s_maskSampler, s_maskTexture);
        bgfx::setTexture(3, s_beamSampler, beam);
        bgfx::setUniform(s_cameraUniform, camera);
        bgfx::setUniform(s_lightUniform, light);
        bgfx::setUniform(s_spotPlaceUniform, s_spotPlace);
        bgfx::setUniform(s_spotAimUniform, s_spotAim);
        bgfx::setUniform(s_frameUniform, frame);
        bgfx::setUniform(s_highlightUniform, highlight);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS);
        bgfx::submit(kModelView, s_foregroundProgram);
    }

    // The glows, added as light: the particles, then the flare
    if (!s_glowQuads.empty() && bgfx::isValid(s_glowTexture)) {
        std::vector<OverlayVertex> points;
        points.reserve(s_glowQuads.size() * 6);
        for (const GlowQuad& q : s_glowQuads) {
            OverlayVertex c[4];
            for (int k = 0; k < 4; k++)
                c[k] = OverlayVertex{ -sx + 2.0f * sx * q.x[k] / 640.0f, sy - 2.0f * sy * q.y[k] / 480.0f, 0, q.u[k], q.v[k],
                                      q.colour };
            points.insert(points.end(), { c[0], c[1], c[2], c[2], c[1], c[3] });
        }
        bgfx::TransientVertexBuffer buffer;
        if (copyPoints(points, s_overlayLayout, buffer)) {
            bgfx::setVertexBuffer(0, &buffer);
            bgfx::setTexture(0, s_overlaySampler, s_glowTexture);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ADD);
            bgfx::submit(kModelView, s_overlayProgram);
        }
    }
    if (!s_glowRects.empty())
        drawRects(s_glowRects, true, s_glowTexture, BGFX_STATE_BLEND_ADD, sx, sy);

    // The cinema bands, over the scene and its glows
    if (s_bandHeight > 0.0f) {
        const std::vector<OverlayRect> bands = {
            { 0, 0, 640, s_bandHeight, 0, 0, 1, 1, 0xFF000000u, false },
            { 0, 480 - s_bandHeight, 640, s_bandHeight, 0, 0, 1, 1, 0xFF000000u, false },
        };
        drawRects(bands, false, s_whiteTexture, BGFX_STATE_BLEND_ALPHA, sx, sy);
    }


    // The layer over everything: the plain rectangles (boxes), then the
    // textured ones (letters)
    if (!s_overlayRects.empty()) {
        drawOverlay(false, sx, sy);
        drawOverlay(true, sx, sy);
    }
    bgfx::frame();
}
