#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Types.h"

#include "Renderer/RendererTypes.h"

namespace Janus
{

struct Sprite
{
    TextureHandle texture;
    Vector2 position{0.0f, 0.0f};
    Vector2 size{1.0f, 1.0f};
    f32 rotationRadians = 0.0f;
    Color color = Color::White();
    i32 layer = 0;
    TextureRegion uv = TextureRegion::Full();
    BlendMode blendMode = BlendMode::Alpha;
};

} // namespace Janus
