#pragma once

// The flashlight's beam, as the game lights with it (a light of type 1,
// FUN_00419d40 makes it, FUN_0041b100 lights with it): a cone from where
// the flashlight is, along where it points. The shaders do the same
// (platform/shaders/flashlight.sh).

// How far the beam reaches (cm; 1875 with the flag 0x505730, not done)
constexpr float kFlashlightReach = 625.0f;

// How much the beam lights a point (0..1): only ahead of it (25 cm on),
// inside its cone (an offset of 100 at its focal of 128, about 38 degrees)
// and weaker to the edge and farther away. place, point: in the world; aim:
// a unit vector
float flashlightStrength(const float place[3], const float aim[3], const float point[3]);
