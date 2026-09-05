#pragma once

#include "Core/Math/Vector2.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/RendererTypes.h"

namespace Janus::Editor
{

class EditorCamera final
{
public:
    void PanPixels(Vector2 deltaPixels) noexcept;
    void Zoom(f32 wheelDelta) noexcept;

    [[nodiscard]] Vector2 ScreenToWorld(
        Vector2 viewportPoint,
        Viewport viewport) const noexcept;

    [[nodiscard]] OrthographicCamera ToRenderCamera() const noexcept;

    [[nodiscard]] Vector2 GetPosition() const noexcept;
    [[nodiscard]] f32 GetZoom() const noexcept;

private:
    Vector2 m_Position{0.0f, 0.0f};
    f32 m_Zoom = 1.0f;
};

} // namespace Janus::Editor
