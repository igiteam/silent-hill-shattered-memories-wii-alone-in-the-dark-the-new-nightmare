#pragma once

// Drawing with bgfx: Direct3D 11, Direct3D 12 or Vulkan.

#include <cstdint>
#include <vector>

enum class RendererChoice {
    Auto,        // Direct3D 11 on Windows
    Direct3D11,
    Direct3D12,
    Vulkan,
};

// Renderer named on the command line: d3d11, d3d12, vulkan (anything else: Auto)
RendererChoice parseRendererChoice(const char* name);

// The one after this one, for switching while the program runs (F2)
RendererChoice nextRendererChoice(RendererChoice choice);

// Starts bgfx on the window with that renderer. False if it cannot
bool initRenderer(RendererChoice choice, int width, int height);

// Stops bgfx and frees what it had
void shutdownRenderer();

// Name of the renderer in use ("Direct3D 11"...)
const char* rendererName();

// The picture shown, in two versions of width x height colours 0xAABBGGRR:
// in the dark and lit. They are kept until changed
void setPictures(int width, int height, const uint32_t* dark, const uint32_t* lit);

// How much the lit version shows over the dark one everywhere (0 none, 1 all)
void setLight(float everywhere);

// The flashlight's beam, in camera space (x right, y depth, z up): where it
// is, where it points (a unit vector) and how far it reaches (cm). Off: no beam
void setFlashlight(bool on, const float place[3], const float aim[3], float reach);

// A point of a 3D mesh in camera space (x right, y depth, z up) and where it
// is on the mesh's textures (0..1)
struct MeshVertex {
    float x, y, z;
    float u, v;
};

// A point of a model in camera space, its normal (also in camera space) and
// where it is on its texture (0..1)
struct ModelPoint {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

// Triangles of a model drawn with one of its textures (-1: none, plain grey)
struct ModelPart {
    int texture;
    std::vector<uint32_t> triangles;  // three point numbers each
};

// A square texture of a model: size x size colours 0xAABBGGRR; alpha 0 is a hole
struct ModelImage {
    int size;
    std::vector<uint32_t> rgba;
};

// Focal length of the camera of the 3D meshes, in pixels of the picture
void setCameraFocal(float focal);

// The mesh of the scene: its points, its triangles (three point numbers
// each; the first roomCount numbers are the room's, the rest the objects of
// the foreground) and a mask of width x height bytes, 255 where an object
// really is and 0 elsewhere. It is drawn twice: right over the picture for
// the flashlight's beam (the room, and the objects only where the mask says),
// and over everything for the foreground (where the mask says). Its colours
// are those of the picture at the same place (lit by the same light)
void setForegroundMesh(const std::vector<MeshVertex>& points, const std::vector<uint32_t>& triangles,
                       size_t roomCount, int width, int height, const uint8_t* mask);

// Textures of the model, kept until changed
void setModelTextures(const std::vector<ModelImage>& textures);

// Model drawn between the picture and the foreground, lit by the same light
// (no parts: none)
void setModel(const std::vector<ModelPoint>& points, const std::vector<ModelPart>& parts);

// Tints the foreground in red, to see where it is
void setForegroundHighlight(bool on);

// The image of the layer drawn over everything (the letters of a font, the
// pictures of a menu): width x height colours 0xAABBGGRR, read pixel by pixel
// (sharp letters of a small font made bigger) or, smooth, blending the
// nearest pixels (pictures drawn at other sizes). Kept until changed
void setOverlayImage(int width, int height, const uint32_t* rgba, bool smooth = false);

// A rectangle of that layer, placed in pixels of the 640x480 picture: a part
// of the layer's image (u, v: 0..1) or, not textured, a plain one, in a colour
// 0xAABBGGRR (its alpha blends it over what is below)
struct OverlayRect {
    float x, y, width, height;
    float u0, v0, u1, v1;
    uint32_t colour;
    bool textured;
    // Turned (radians, clockwise on the screen) about the middle of its left
    // side: a ray from there
    float angle = 0;
};

// What the layer shows this frame, drawn in this order (none: nothing)
void setOverlay(const std::vector<OverlayRect>& rects);

// The light of the models (FUN_00426070): the level's ambient colour (0..1
// each; +0x24 or +0x28 of its DRAM) and the scene's light (its place in camera
// space and its strength 0..1), on the sides facing it
void setModelLight(const float ambient[3], const float lightPlace[3], float lightStrength);

// The shadows on the floor: triangles in pixels of the 640 x 480 picture (x,
// y, three points each), darkened that much (0..1) once wherever they lie,
// under the models and the foreground
void setShadows(const std::vector<float>& triangles, float darkness);

// The black bands of the scenes (cinema): that many pixels of the 640 x 480
// picture at its top and at its bottom (0: none)
void setCinemaBands(float height);

// The glows: rectangles like the layer's, of their own image, added to what
// is below as light (the flare of a flashlight), drawn before the layer
void setGlowImage(int width, int height, const uint32_t* rgba);
void setGlows(const std::vector<OverlayRect>& rects);

// A free four-sided piece of the glows (particles: rain, lightning...):
// its corners in pixels of the 640 x 480 picture, in the order top left, top
// right, bottom left, bottom right, with their places in the glows' image
// (0..1), in a colour 0xAABBGGRR, added as light
struct GlowQuad {
    float x[4], y[4];
    float u[4], v[4];
    uint32_t colour;
};
void setGlowQuads(const std::vector<GlowQuad>& quads);

// Point of the picture (0..1 of its width and height) under a point of the
// window in pixels. False if it is outside the picture
bool pictureFromWindow(float windowX, float windowY, float& x, float& y);

// Draws one frame: the picture at 4:3 in the middle, black bands at the sides,
// then the model, the foreground and the layer over everything
void drawFrame(int windowWidth, int windowHeight);
