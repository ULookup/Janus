#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Types.h"
#include <unordered_map>

namespace Janus
{
struct FontGlyph
{
    f32 x = 0, y = 0, width = 0, height = 0;
    f32 offsetX = 0, offsetY = 0, advance = 0;
};

// CPU metrics only. The atlas remains a separately cached Texture asset.
struct FontAsset
{
    AssetHandle atlas;
    u32 width = 0, height = 0;
    f32 lineHeight = 0, baseline = 0;
    u32 fallback = 0;
    std::unordered_map<u32, FontGlyph> glyphs;
};
} // namespace Janus
