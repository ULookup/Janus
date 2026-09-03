#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Math/Mat4.h"
#include "Core/Math/Vector2.h"
#include "Core/Types.h"
#include "Core/UUID/UUID.h"
#include "Renderer/RendererTypes.h"

#include <string>

namespace Janus
{

struct EntityIdentityComponent
{
    UUID id;
    std::string name{"Entity"};
};

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
    AssetHandle texture;
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

struct LuaScriptComponent
{
    AssetHandle script;
    bool enabled = true;
};

} // namespace Janus
