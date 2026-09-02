#pragma once

#include "Core/Math/Mat4.h"
#include "Core/Math/Vector2.h"
#include "Core/Types.h"
#include "Renderer/RendererTypes.h"

namespace Janus
{

struct TransformComponent
{
    Vector2 position;
    f32 rotationRadians = 0.0f;
    Vector2 scale{1.0f, 1.0f};

    Vector2 worldPosition;
    f32 worldRotationRadians = 0.0f;
    Vector2 worldScale{1.0f, 1.0f};
    bool dirty = true;
};

struct SpriteRendererComponent
{
    TextureHandle texture;
    Vector2 size{1.0f, 1.0f};
    Color color = Color::White();
    i32 layer = 0;
    TextureRegion uv = TextureRegion::Full();
    bool enabled = true;
};

struct CameraComponent
{
    f32 zoom = 1.0f;
    bool primary = false;
};

} // namespace Janus
