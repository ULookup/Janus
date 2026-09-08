#include "Scene/ScenePose.h"

#include "Scene/Scene.h"

#include <cmath>
#include <vector>

namespace Janus
{
namespace
{
bool IsFinite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}
} // namespace

Result<SceneWorldPose> ScenePose::Resolve(const Scene& scene, UUID id,
                                          std::optional<ScenePositionOverride> preview)
{
    auto entity = scene.FindEntity(id);
    if (!entity.IsValid())
        return Result<SceneWorldPose>::Failure(ErrorCode::EntityNotFound,
                                               "Cannot resolve a missing Scene entity pose.");
    if (preview)
    {
        const auto target = scene.FindEntity(preview->entity);
        if (!target.IsValid() || !scene.HasComponent<TransformComponent>(target))
            return Result<SceneWorldPose>::Failure(ErrorCode::EntityNotFound,
                                                   "Scene position preview target is missing.");
        if (!IsFinite(preview->position))
            return Result<SceneWorldPose>::Failure(ErrorCode::InvalidArgument,
                                                   "Scene position preview must be finite.");
    }

    std::vector<ECS::Entity> chain;
    const auto limit = scene.GetRegistry().AliveEntityCount();
    while (entity.IsValid())
    {
        // A parent chain cannot visit more nodes than the entire live Scene.
        if (chain.size() >= limit)
            return Result<SceneWorldPose>::Failure(ErrorCode::HierarchyCycle,
                                                   "Scene pose parent chain contains a cycle.");
        const auto* hierarchy = scene.GetComponent<HierarchyComponent>(entity);
        if (!hierarchy || !scene.HasComponent<TransformComponent>(entity) ||
            !scene.HasComponent<EntityIdentityComponent>(entity))
            return Result<SceneWorldPose>::Failure(ErrorCode::InvalidState,
                                                   "Scene pose parent chain is incomplete.");
        chain.push_back(entity);
        entity = hierarchy->parent;
    }

    SceneWorldPose result;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        const auto& transform = *scene.GetComponent<TransformComponent>(*it);
        const auto entityId = scene.GetComponent<EntityIdentityComponent>(*it)->id;
        const auto position =
            preview && preview->entity == entityId ? preview->position : transform.position;
        if (!IsFinite(position) || !IsFinite(transform.scale) ||
            !std::isfinite(transform.rotationRadians))
            return Result<SceneWorldPose>::Failure(ErrorCode::InvalidArgument,
                                                   "Scene local pose must be finite.");
        const auto local = Mat4::Multiply(
            Mat4::Translate(position),
            Mat4::Multiply(Mat4::Rotate(transform.rotationRadians), Mat4::Scale(transform.scale)));
        result.position = Mat4::TransformPoint(result.matrix, position);
        result.matrix = Mat4::Multiply(result.matrix, local);
        // Preserve Sprite's existing rotation/scale representation, including shear approximation.
        result.rotationRadians += transform.rotationRadians;
        result.scale = {result.scale.x * transform.scale.x, result.scale.y * transform.scale.y};
        if (!IsFinite(result.position) || !IsFinite(result.scale) ||
            !std::isfinite(result.rotationRadians))
            return Result<SceneWorldPose>::Failure(ErrorCode::InvalidArgument,
                                                   "Scene world pose exceeds finite bounds.");
        for (usize index = 0; index < 16; ++index)
            if (!std::isfinite(result.matrix.Data()[index]))
                return Result<SceneWorldPose>::Failure(ErrorCode::InvalidArgument,
                                                       "Scene world matrix exceeds finite bounds.");
    }
    return Result<SceneWorldPose>::Success(result);
}

} // namespace Janus
