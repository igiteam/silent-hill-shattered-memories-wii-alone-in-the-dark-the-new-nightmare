#include "flashlight.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kFocal = 128.0f;  // 160 / tan(1.7921 / 2): +0xD4 of the light
constexpr float kCone = 100.0f;   // its cone, squared 10000 (1250 narrow, flag 0x505731, not done)
constexpr float kNearest = 25.0f;

} // namespace

float flashlightStrength(const float place[3], const float aim[3], const float point[3])
{
    const float away[3] = { point[0] - place[0], point[1] - place[1], point[2] - place[2] };
    const float depth = away[0] * aim[0] + away[1] * aim[1] + away[2] * aim[2];
    if (depth <= kNearest || depth >= kFlashlightReach)
        return 0.0f;
    const float side[3] = { away[0] - depth * aim[0], away[1] - depth * aim[1], away[2] - depth * aim[2] };
    const float offset = std::sqrt(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]) * kFocal / depth;
    return std::clamp(2.0f * (1.0f - offset * offset / (kCone * kCone) - depth / kFlashlightReach), 0.0f, 1.0f);
}
