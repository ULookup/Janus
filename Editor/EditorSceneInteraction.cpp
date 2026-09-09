#include "Panels/InspectorPanel.h"
#include "EditorActions.h"
#include "EditorApplication.h"
#include "EditorCamera.h"
#include "EditorContext.h"
#include "ProjectSession.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/ScenePose.h"
#include "UI/UIComponents.h"

#include <cmath>
#include <imgui.h>

namespace Janus::Editor
{
namespace
{
void DrawSceneGrid(const EditorCamera& camera, Viewport viewport, ImVec2 rectMin, ImVec2 rectMax)
{
    if (viewport.width == 0 || viewport.height == 0)
    {
        return;
    }

    const f32 zoom = camera.GetZoom();
    if (zoom <= 0.0f)
    {
        return;
    }

    const f32 worldSpacing = camera.GetGridSpacing();

    const Vector2 worldTopLeft = camera.ScreenToWorld(Vector2{0.0f, 0.0f}, viewport);
    const Vector2 worldBottomRight = camera.ScreenToWorld(
        Vector2{static_cast<f32>(viewport.width), static_cast<f32>(viewport.height)}, viewport);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(rectMin, rectMax, true);

    const ImU32 gridColor = ImGui::GetColorU32(ImGuiCol_Border, 0.35f);
    const ImU32 axisColor = ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.65f);

    const ImU32 minorColor = ImGui::GetColorU32(ImGuiCol_Border, .16f);
    const f32 minorSpacing = worldSpacing * .25f;
    const auto lineColor = [&](f32 value)
    {
        if (std::abs(value) < .001f)
            return axisColor;
        const f32 majorIndex = value / worldSpacing;
        return std::abs(majorIndex - std::round(majorIndex)) < .001f ? gridColor : minorColor;
    };

    const f32 firstX = std::floor(worldTopLeft.x / minorSpacing) * minorSpacing;

    // Integer iteration stays bounded even where world coordinates lose float precision.
    const int columns = static_cast<int>(viewport.width * zoom / minorSpacing) + 2;
    for (int index = 0; index < columns; ++index)
    {
        const f32 worldX = firstX + index * minorSpacing;
        const f32 screenX =
            rectMin.x + (worldX - worldTopLeft.x) / zoom * (rectMax.x - rectMin.x) / viewport.width;

        drawList->AddLine(ImVec2{screenX, rectMin.y}, ImVec2{screenX, rectMax.y},
                          lineColor(worldX));
    }

    const f32 firstY = std::floor(worldBottomRight.y / minorSpacing) * minorSpacing;

    const int rows = static_cast<int>(viewport.height * zoom / minorSpacing) + 2;
    for (int index = 0; index < rows; ++index)
    {
        const f32 worldY = firstY + index * minorSpacing;
        const f32 screenY = rectMin.y + (worldTopLeft.y - worldY) / zoom * (rectMax.y - rectMin.y) /
                                            viewport.height;

        drawList->AddLine(ImVec2{rectMin.x, screenY}, ImVec2{rectMax.x, screenY},
                          lineColor(worldY));
    }

    drawList->PopClipRect();
}

} // namespace

bool EditorApplication::DrawSceneInteraction(Vector2 origin, Vector2 size, bool hovered)
{
    auto& io = ImGui::GetIO();
    const bool wasDragging = m_TransformDrag.IsActive();
    const bool blocked = m_CloseRequested || m_ProjectSession->IsClosePending() ||
                         io.WantTextInput || ImGui::IsAnyItemActive() ||
                         (m_InspectorPanel && m_InspectorPanel->OwnsKeyboardInput()) ||
                         ImGui::GetDragDropPayload() != nullptr ||
                         ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    if (blocked)
    {
        m_TransformDrag.Cancel();
        return true;
    }

    if (hovered)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Q))
        {
            m_MoveTool = false;
            m_TransformDrag.Cancel();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_W))
            m_MoveTool = true;
        if (ImGui::IsKeyPressed(ImGuiKey_F))
            FrameScene(true);
        if (io.MouseWheel != 0 || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        {
            m_TransformDrag.Cancel();
            m_EditorCamera->Zoom(io.MouseWheel);
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                m_EditorCamera->PanPixels({io.MouseDelta.x * m_SceneViewViewport.width / size.x,
                                           io.MouseDelta.y * m_SceneViewViewport.height / size.y});
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        m_TransformDrag.Cancel();

    if (m_ShowGrid)
        DrawSceneGrid(*m_EditorCamera, m_SceneViewViewport, {origin.x, origin.y},
                      {origin.x + size.x, origin.y + size.y});

    const auto selected = m_EditorContext->selection.GetSelectedUUID();
    if (!selected)
    {
        m_TransformDrag.Cancel();
        return wasDragging;
    }
    auto& scene = m_ProjectSession->GetEditorScene();
    const auto entity = scene.FindEntity(*selected);
    const bool layoutEntity =
        scene.HasComponent<UIRectComponent>(entity) || scene.HasComponent<CanvasComponent>(entity);
    const bool editable = !layoutEntity && !m_ProjectSession->IsAuthoringReadOnly();
    if (!editable ||
        (m_TransformDrag.GetPreview() && m_TransformDrag.GetPreview()->entity != *selected))
        m_TransformDrag.Cancel();

    const TransformDragView view{*m_EditorCamera, m_SceneViewViewport, origin, size, m_UiScale};
    const Vector2 mouse{io.MousePos.x, io.MousePos.y};
    if (m_TransformDrag.IsActive())
    {
        const auto updated = m_TransformDrag.Update(*m_ProjectSession, mouse, view, io.KeyCtrl,
                                                    m_EditorCamera->GetGridSpacing());
        if (!updated)
            RecordError(updated.GetError());
        else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            const auto committed = m_EditorActions->CommitTransformDrag(m_TransformDrag);
            if (!committed)
                RecordError(committed.GetError());
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            m_TransformDrag.Cancel();
    }

    const auto pose = ScenePose::Resolve(scene, *selected, m_TransformDrag.GetPreview());
    if (!pose)
        return wasDragging;
    const auto projectPoint = [&](Vector2 world)
    {
        const auto center = m_EditorCamera->GetPosition();
        const auto zoom = m_EditorCamera->GetZoom();
        return ImVec2{origin.x + size.x * .5f +
                          (world.x - center.x) / zoom * size.x / m_SceneViewViewport.width,
                      origin.y + size.y * .5f -
                          (world.y - center.y) / zoom * size.y / m_SceneViewViewport.height};
    };
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect({origin.x, origin.y}, {origin.x + size.x, origin.y + size.y}, true);
    if (const auto* sprite = scene.GetComponent<SpriteRendererComponent>(entity);
        sprite && !layoutEntity)
    {
        const auto& value = pose.Value();
        const float c = std::cos(value.rotationRadians), s = std::sin(value.rotationRadians);
        ImVec2 points[4];
        constexpr Vector2 corners[] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (int i = 0; i < 4; ++i)
        {
            const float x = corners[i].x * sprite->size.x * value.scale.x * .5f;
            const float y = corners[i].y * sprite->size.y * value.scale.y * .5f;
            points[i] =
                projectPoint({value.position.x + x * c - y * s, value.position.y + x * s + y * c});
        }
        draw->AddPolyline(points, 4, IM_COL32(90, 180, 255, 255), ImDrawFlags_Closed,
                          2 * m_UiScale);
    }

    bool consumed = wasDragging || m_TransformDrag.IsActive();
    if (m_MoveTool && !editable)
        draw->AddText({origin.x + 12 * m_UiScale, origin.y + 12 * m_UiScale},
                      ImGui::GetColorU32(ImGuiCol_TextDisabled),
                      EditorText(m_Preferences.language,
                                 layoutEntity ? "UI uses layout properties in Inspector."
                                              : "Move is unavailable while authoring is locked."));
    if (m_MoveTool && editable)
    {
        const auto center = projectPoint(pose.Value().position);
        const float unit = m_UiScale;
        const float x = (mouse.x - center.x) / unit;
        const float y = (center.y - mouse.y) / unit;
        const bool hitPlane = x >= 8 && x <= 26 && y >= 8 && y <= 26;
        const bool hitX = x >= 0 && x <= 78 && std::abs(y) <= 7;
        const bool hitY = y >= 0 && y <= 78 && std::abs(x) <= 7;
        const bool hit = hovered && (hitPlane || hitX || hitY);
        const auto axis = hitPlane ? TransformDragAxis::XY
                          : hitX   ? TransformDragAxis::X
                                   : TransformDragAxis::Y;
        const auto xColor = IM_COL32(240, 95, 90, 255);
        const auto yColor = IM_COL32(105, 220, 135, 255);
        draw->AddLine(center, {center.x + 70 * unit, center.y}, xColor, 3 * unit);
        draw->AddTriangleFilled({center.x + 78 * unit, center.y},
                                {center.x + 66 * unit, center.y - 5 * unit},
                                {center.x + 66 * unit, center.y + 5 * unit}, xColor);
        draw->AddLine(center, {center.x, center.y - 70 * unit}, yColor, 3 * unit);
        draw->AddTriangleFilled({center.x, center.y - 78 * unit},
                                {center.x - 5 * unit, center.y - 66 * unit},
                                {center.x + 5 * unit, center.y - 66 * unit}, yColor);
        draw->AddRectFilled({center.x + 8 * unit, center.y - 26 * unit},
                            {center.x + 26 * unit, center.y - 8 * unit},
                            hitPlane ? IM_COL32(245, 210, 90, 230) : IM_COL32(245, 210, 90, 110));
        draw->AddText({center.x + 80 * unit, center.y - 9 * unit}, xColor, "X");
        draw->AddText({center.x - 4 * unit, center.y - 98 * unit}, yColor, "Y");
        if (hit)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            if (!consumed && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !ImGui::IsAnyItemActive())
            {
                const auto began =
                    m_TransformDrag.Begin(*m_ProjectSession, *selected, axis, mouse, view);
                if (!began)
                    RecordError(began.GetError());
                else
                    m_SuppressGameUntilReleased = true;
                consumed = true;
            }
        }
    }
    draw->PopClipRect();
    return consumed;
}

} // namespace Janus::Editor
