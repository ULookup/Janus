#include "EditorTransformDrag.h"

#include "ProjectSession.h"
#include "Scene/Command/SceneCommands.h"
#include "Scene/Scene.h"
#include "UI/UIComponents.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace Janus::Editor
{
namespace
{
bool Finite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool Same(Vector2 a, Vector2 b)
{
    return a.x == b.x && a.y == b.y;
}

bool ValidView(const TransformDragView& view)
{
    return view.viewport.width > 0 && view.viewport.height > 0 && Finite(view.displayMin) &&
           Finite(view.displaySize) && view.displaySize.x > 0 && view.displaySize.y > 0 &&
           std::isfinite(view.dpiScale) && view.dpiScale > 0 && Finite(view.camera.GetPosition()) &&
           std::isfinite(view.camera.GetZoom()) && view.camera.GetZoom() > 0;
}

bool SameView(const TransformDragView& a, const TransformDragView& b)
{
    return Same(a.camera.GetPosition(), b.camera.GetPosition()) &&
           a.camera.GetZoom() == b.camera.GetZoom() && a.viewport.width == b.viewport.width &&
           a.viewport.height == b.viewport.height && Same(a.displayMin, b.displayMin) &&
           Same(a.displaySize, b.displaySize) && a.dpiScale == b.dpiScale;
}

Vector2 WorldPoint(Vector2 screenPoint, const TransformDragView& view)
{
    const Vector2 pixels{(screenPoint.x - view.displayMin.x) *
                             static_cast<f32>(view.viewport.width) / view.displaySize.x,
                         (screenPoint.y - view.displayMin.y) *
                             static_cast<f32>(view.viewport.height) / view.displaySize.y};
    return view.camera.ScreenToWorld(pixels, view.viewport);
}

Result<Vector2> InversePoint(const Mat4& matrix, Vector2 world)
{
    const auto* m = matrix.Data();
    for (usize index = 0; index < 16; ++index)
        if (!std::isfinite(m[index]))
            return Result<Vector2>::Failure(ErrorCode::InvalidArgument,
                                            "Move parent matrix is not finite.");
    const double a = m[0], b = m[4], c = m[1], d = m[5];
    const double determinant = a * d - b * c;
    const double magnitude = std::max({std::abs(a), std::abs(b), std::abs(c), std::abs(d)});
    // A relative check catches badly conditioned parents, even when nested rotation
    // and nonuniform scale produce shear. An absolute floor bounds tiny transforms.
    if (!std::isfinite(determinant) || std::abs(determinant) <= 1e-12 ||
        std::abs(determinant) <= magnitude * magnitude * 1e-6)
        return Result<Vector2>::Failure(ErrorCode::InvalidArgument,
                                        "Move parent transform is singular or nearly singular.");
    const double x = static_cast<double>(world.x) - m[12];
    const double y = static_cast<double>(world.y) - m[13];
    const Vector2 local{static_cast<f32>((d * x - b * y) / determinant),
                        static_cast<f32>((a * y - c * x) / determinant)};
    if (!Finite(local))
        return Result<Vector2>::Failure(ErrorCode::InvalidArgument, "Move position is not finite.");
    return Result<Vector2>::Success(local);
}
} // namespace

Result<void> EditorTransformDrag::Fail(Error error)
{
    Cancel();
    return Result<void>::Failure(std::move(error));
}

void EditorTransformDrag::Cancel() noexcept
{
    m_Preview.reset();
    m_Chain.clear();
}

Result<void> EditorTransformDrag::Begin(ProjectSession& project, UUID entity,
                                        TransformDragAxis axis, Vector2 screenPoint,
                                        const TransformDragView& view)
{
    Cancel();
    if (project.IsAuthoringReadOnly())
        return Fail({ErrorCode::InvalidState, "Move requires an editable stopped scene."});
    if (!ValidView(view) || !Finite(screenPoint) ||
        (axis != TransformDragAxis::X && axis != TransformDragAxis::Y &&
         axis != TransformDragAxis::XY))
        return Fail({ErrorCode::InvalidArgument, "Move requires a valid viewport and pointer."});
    const auto& scene = project.GetEditorScene();
    auto current = scene.FindEntity(entity);
    if (!current.IsValid())
        return Fail({ErrorCode::EntityNotFound, "Move target no longer exists."});
    if (scene.HasComponent<CanvasComponent>(current) ||
        scene.HasComponent<UIRectComponent>(current))
        return Fail({ErrorCode::InvalidArgument,
                     "Canvas and UIRect use layout properties instead of Move."});
    const auto pose = ScenePose::Resolve(scene, entity);
    if (!pose)
        return Fail(pose.GetError());
    // Store UUIDs and local values only. A replaced Scene cannot leave dangling pointers.
    while (current.IsValid())
    {
        if (m_Chain.size() >= 1024)
            return Fail({ErrorCode::InvalidState, "Move parent hierarchy exceeds its bound."});
        const auto* identity = scene.GetComponent<EntityIdentityComponent>(current);
        const auto* transform = scene.GetComponent<TransformComponent>(current);
        const auto* hierarchy = scene.GetComponent<HierarchyComponent>(current);
        if (!identity || !transform || !hierarchy || !Finite(transform->position) ||
            !Finite(transform->scale) || !std::isfinite(transform->rotationRadians))
            return Fail({ErrorCode::InvalidState, "Move hierarchy has invalid local data."});
        UUID parent;
        if (hierarchy->parent.IsValid())
        {
            const auto* parentIdentity =
                scene.GetComponent<EntityIdentityComponent>(hierarchy->parent);
            if (!parentIdentity)
                return Fail({ErrorCode::InvalidState, "Move hierarchy has an invalid parent."});
            parent = parentIdentity->id;
        }
        m_Chain.push_back({identity->id, parent, transform->position, transform->rotationRadians,
                           transform->scale});
        current = hierarchy->parent;
    }
    m_ParentMatrix = Mat4::Identity();
    if (m_Chain.front().parent.IsValid())
    {
        const auto parentPose = ScenePose::Resolve(scene, m_Chain.front().parent);
        if (!parentPose)
            return Fail(parentPose.GetError());
        m_ParentMatrix = parentPose.Value().matrix;
    }
    const auto inverse = InversePoint(m_ParentMatrix, pose.Value().position);
    if (!inverse)
        return Fail(inverse.GetError());
    m_PointerStart = WorldPoint(screenPoint, view);
    if (!Finite(m_PointerStart))
        return Fail({ErrorCode::InvalidArgument, "Move pointer mapping is not finite."});
    m_ProjectIdentity = project.GetProjectIdentity();
    m_Revision = project.GetSceneRevision();
    m_Generation = project.GetAuthoringGeneration();
    m_Axis = axis;
    m_View = view;
    m_WorldStart = pose.Value().position;
    m_WorldTarget = m_WorldStart;
    m_LocalStart = m_Chain.front().position;
    m_Preview = ScenePositionOverride{entity, m_LocalStart};
    return Result<void>::Success();
}

Result<void> EditorTransformDrag::ValidateSession(ProjectSession& project)
{
    if (!IsActive())
        return Fail({ErrorCode::InvalidState, "No Move capture is active."});
    if (project.GetProjectIdentity() != m_ProjectIdentity ||
        project.GetSceneRevision() != m_Revision ||
        project.GetAuthoringGeneration() != m_Generation || project.IsAuthoringReadOnly())
        return Fail(
            {ErrorCode::InvalidState, "Move cancelled because the authoring session changed."});
    const auto& scene = project.GetEditorScene();
    const auto target = scene.FindEntity(m_Preview->entity);
    if (scene.HasComponent<CanvasComponent>(target) || scene.HasComponent<UIRectComponent>(target))
        return Fail({ErrorCode::InvalidState, "Move target now uses screen-space layout."});
    for (const auto& captured : m_Chain)
    {
        const auto entity = scene.FindEntity(captured.entity);
        const auto* transform = scene.GetComponent<TransformComponent>(entity);
        const auto* hierarchy = scene.GetComponent<HierarchyComponent>(entity);
        if (!transform || !hierarchy || !Same(transform->position, captured.position) ||
            !Same(transform->scale, captured.scale) ||
            transform->rotationRadians != captured.rotation)
            return Fail(
                {ErrorCode::InvalidState, "Move target or parent local transform changed."});
        const auto* parent = scene.GetComponent<EntityIdentityComponent>(hierarchy->parent);
        if ((hierarchy->parent.IsValid() && !parent) ||
            (parent ? parent->id : UUID{}) != captured.parent)
            return Fail({ErrorCode::InvalidState, "Move parent hierarchy changed."});
    }
    return Result<void>::Success();
}

Result<void> EditorTransformDrag::Validate(ProjectSession& project, const TransformDragView& view)
{
    const auto valid = ValidateSession(project);
    if (!valid)
        return valid;
    if (!ValidView(view) || !SameView(m_View, view))
        return Fail(
            {ErrorCode::InvalidState, "Move cancelled because the camera or viewport changed."});
    return Result<void>::Success();
}

Result<void> EditorTransformDrag::Update(ProjectSession& project, Vector2 screenPoint,
                                         const TransformDragView& view, bool snap, f32 gridSpacing)
{
    const auto valid = Validate(project, view);
    if (!valid)
        return valid;
    if (!Finite(screenPoint) || (snap && (!std::isfinite(gridSpacing) || gridSpacing <= 0)))
        return Fail({ErrorCode::InvalidArgument, "Move pointer or snap grid is invalid."});
    const auto point = WorldPoint(screenPoint, view);
    if (!Finite(point))
        return Fail({ErrorCode::InvalidArgument, "Move pointer mapping is not finite."});
    Vector2 delta{point.x - m_PointerStart.x, point.y - m_PointerStart.y};
    if (m_Axis == TransformDragAxis::X)
        delta.y = 0;
    if (m_Axis == TransformDragAxis::Y)
        delta.x = 0;
    if (snap)
    {
        delta.x = std::round(delta.x / gridSpacing) * gridSpacing;
        delta.y = std::round(delta.y / gridSpacing) * gridSpacing;
    }
    m_WorldTarget = {m_WorldStart.x + delta.x, m_WorldStart.y + delta.y};
    if (!Finite(m_WorldTarget))
        return Fail({ErrorCode::InvalidArgument, "Move world position is not finite."});
    if (delta.x == 0 && delta.y == 0)
        m_Preview->position = m_LocalStart;
    else
    {
        const auto local = InversePoint(m_ParentMatrix, m_WorldTarget);
        if (!local)
            return Fail(local.GetError());
        m_Preview->position = local.Value();
    }
    return Result<void>::Success();
}

Result<void> EditorTransformDrag::Commit(ProjectSession& project)
{
    const auto valid = ValidateSession(project);
    if (!valid)
        return valid;
    const auto preview = *m_Preview;
    const bool moved = !Same(preview.position, m_LocalStart);
    Cancel();
    if (!moved)
        return Result<void>::Success();
    return project.ExecuteAuthoringIfCurrent(
        std::make_unique<SetPropertyCommand>(
            project.GetEditorScene(),
            SceneReflection(project.GetReflectionRegistry(), &project.GetAssetRegistry()),
            preview.entity, SceneReflectionIds::Transform, SceneReflectionIds::TransformPosition,
            preview.position),
        m_Revision, m_Generation);
}
} // namespace Janus::Editor
