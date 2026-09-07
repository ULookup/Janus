#pragma once
#include "Asset/AssetHandle.h"
#include "Core/Math/Vector2.h"
#include <vector>

namespace Janus
{
struct AnimationFrame
{
    Vector2 uvMin{0, 0};
    Vector2 uvMax{1, 1};
    f64 duration = 0;
};

// CPU-only atlas coordinates. Texture ownership stays with AssetService/Renderer2D.
struct AnimationClip
{
    AssetHandle texture;
    bool loop = false;
    std::vector<AnimationFrame> frames;
    f64 duration = 0;
};
} // namespace Janus
