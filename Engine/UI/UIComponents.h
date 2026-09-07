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

// OnClick is the fixed callback on this entity's enabled LuaScript.
struct ButtonComponent
{
    bool enabled = true;
    bool interactable = true;
    ColorValue color{0.15f, 0.25f, 0.4f, 1};
    ColorValue focusedColor{0.25f, 0.5f, 0.8f, 1};
    ColorValue pressedColor{0.1f, 0.65f, 0.55f, 1};
    ColorValue disabledColor{0.25f, 0.25f, 0.25f, 1};
};

struct TextComponent
{
    AssetReferenceValue font;
    std::string content;
    f32 fontSize = 24;
    ColorValue color;
    std::string alignment = "left";
    bool enabled = true;
};

[[nodiscard]] Result<void> ValidateUIRect(const UIRectComponent& rect);
} // namespace Janus
