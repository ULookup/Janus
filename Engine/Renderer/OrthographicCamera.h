#pragma once

#include "Core/Math/Mat4.h"
#include "Core/Math/Vector2.h"
#include "Core/Types.h"

#include "Renderer/RendererTypes.h"

namespace Janus
{

struct OrthographicCamera
{
    Vector2 position{0.0f, 0.0f};
    f32 rotationRadians = 0.0f;
    f32 zoom = 1.0f;

    [[nodiscard]] Mat4 ViewProjection(Viewport viewport) const
    {
        const f32 width = static_cast<f32>(viewport.width);
        const f32 height = static_cast<f32>(viewport.height);
        const f32 halfWidth = width * 0.5f;
        const f32 halfHeight = height * 0.5f;

        const auto projection = Mat4::Orthographic(
            -halfWidth,
            halfWidth,
            -halfHeight,
            halfHeight,
            -1.0f,
            1.0f);

        // The camera looks at its position; zoom scales the visible world
        // extent around that point. The inverse scale in the view matrix
        // makes larger zoom values show more of the world.
        const auto toCamera =
            Mat4::Multiply(
                Mat4::Rotate(-rotationRadians),
                Mat4::Translate(Vector2{-position.x, -position.y}));
        const f32 inverseZoom = 1.0f / zoom;
        const auto zoomed = Mat4::Multiply(
            Mat4::Scale(Vector2{inverseZoom, inverseZoom}),
            toCamera);

        return Mat4::Multiply(projection, zoomed);
    }
};

} // namespace Janus
