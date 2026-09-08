#pragma once

#include "Core/Error/Result.h"
#include "EditorCamera.h"
#include "Scene/ScenePose.h"

#include <optional>
#include <vector>

namespace Janus::Editor
{
class ProjectSession;

enum class TransformDragAxis
{
    X,
    Y,
    XY
};

// Pointer and display rectangle use the same UI coordinates. viewport is the actual
// render-target pixel extent; dpiScale detects display changes during capture.
struct TransformDragView
{
    EditorCamera camera;
    Viewport viewport;
    Vector2 displayMin;
    Vector2 displaySize;
    f32 dpiScale = 1;
};

class EditorTransformDrag final
{
  public:
    Result<void> Begin(ProjectSession& project, UUID entity, TransformDragAxis axis,
                       Vector2 screenPoint, const TransformDragView& view);
    Result<void> Update(ProjectSession& project, Vector2 screenPoint, const TransformDragView& view,
                        bool snap = false, f32 gridSpacing = 1);
    Result<void> Validate(ProjectSession& project, const TransformDragView& view);
    Result<void> Commit(ProjectSession& project);
    void Cancel() noexcept;
    bool IsActive() const noexcept
    {
        return m_Preview.has_value();
    }
    std::optional<ScenePositionOverride> GetPreview() const noexcept
    {
        return m_Preview;
    }
    Vector2 GetWorldTarget() const noexcept
    {
        return m_WorldTarget;
    }

  private:
    struct ChainEntry
    {
        UUID entity;
        UUID parent;
        Vector2 position;
        f32 rotation = 0;
        Vector2 scale;
    };
    Result<void> ValidateSession(ProjectSession& project);
    Result<void> Fail(Error error);
    UUID m_ProjectIdentity;
    u64 m_Revision = 0;
    u64 m_Generation = 0;
    TransformDragAxis m_Axis = TransformDragAxis::XY;
    TransformDragView m_View;
    Vector2 m_PointerStart;
    Vector2 m_WorldStart;
    Vector2 m_WorldTarget;
    Vector2 m_LocalStart;
    Mat4 m_ParentMatrix;
    std::vector<ChainEntry> m_Chain;
    std::optional<ScenePositionOverride> m_Preview;
};
} // namespace Janus::Editor
