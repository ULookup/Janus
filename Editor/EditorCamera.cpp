#include "EditorCamera.h"

#include <algorithm>
#include <cmath>

namespace Janus::Editor
{

namespace
{

constexpr f32 MinZoom = 0.05f;
constexpr f32 MaxZoom = 20.0f;
constexpr f32 WheelZoomBase = 0.9f;

} // namespace

void EditorCamera::PanPixels(Vector2 deltaPixels) noexcept
{
    m_Position.x -= deltaPixels.x * m_Zoom;
    m_Position.y += deltaPixels.y * m_Zoom;
}

void EditorCamera::Zoom(f32 wheelDelta) noexcept
{
    if (wheelDelta == 0.0f)
    {
        return;
    }

    m_Zoom *= std::pow(WheelZoomBase, wheelDelta);
    m_Zoom = std::clamp(m_Zoom, MinZoom, MaxZoom);
}

Vector2 EditorCamera::ScreenToWorld(
    Vector2 viewportPoint,
    Viewport viewport) const noexcept
{
    const f32 halfWidth =
        static_cast<f32>(viewport.width) * 0.5f;
    const f32 halfHeight =
        static_cast<f32>(viewport.height) * 0.5f;

    return Vector2{
        m_Position.x
            + (viewportPoint.x - halfWidth) * m_Zoom,
        m_Position.y
            + (halfHeight - viewportPoint.y) * m_Zoom};
}

OrthographicCamera EditorCamera::ToRenderCamera() const noexcept
{
    OrthographicCamera camera;
    camera.position = m_Position;
    camera.zoom = m_Zoom;
    return camera;
}

Vector2 EditorCamera::GetPosition() const noexcept
{
    return m_Position;
}

f32 EditorCamera::GetZoom() const noexcept
{
    return m_Zoom;
}

} // namespace Janus::Editor
