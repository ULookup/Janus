#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Error/Result.h"
#include "Core/Reflection/ReflectionTypes.h"

namespace Janus
{
struct CanvasComponent
{
    bool enabled = true;
};

// Local, top-left coordinates in the host's logical resolution.
// Resolved size = parent size * (anchorMax - anchorMin) + size.
struct UIRectComponent
{
    Vector2 anchorMin;
    Vector2 anchorMax;
    Vector2 pivot;
    Vector2 offset;
    Vector2 size{100, 60};
    bool enabled = true;
    bool clipChildren = false;
};

struct PanelComponent
{
    ColorValue color;
    bool enabled = true;
};

struct ImageComponent
{
    AssetReferenceValue texture;
    ColorValue color;
    Vector2 uvMin;
    Vector2 uvMax{1, 1};
    bool enabled = true;
};

[[nodiscard]] Result<void> ValidateUIRect(const UIRectComponent& rect);
} // namespace Janus
