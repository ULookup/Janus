#include "ScenePicker.h"

#include "EditorCamera.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <cmath>
#include <limits>

namespace Janus::Editor
{

namespace
{

[[nodiscard]] bool ContainsPoint(
    const TransformComponent& transform,
    const SpriteRendererComponent& sprite,
    Vector2 point) noexcept
{
    const f32 deltaX =
        point.x - transform.worldPosition.x;
    const f32 deltaY =
        point.y - transform.worldPosition.y;

    const f32 cosine =
        std::cos(transform.worldRotationRadians);
    const f32 sine =
        std::sin(transform.worldRotationRadians);

    const f32 localX =
        deltaX * cosine + deltaY * sine;
    const f32 localY =
        -deltaX * sine + deltaY * cosine;

    const f32 halfWidth =
        std::abs(sprite.size.x * transform.worldScale.x)
        * 0.5f;
    const f32 halfHeight =
        std::abs(sprite.size.y * transform.worldScale.y)
        * 0.5f;

    return std::abs(localX) <= halfWidth
        && std::abs(localY) <= halfHeight;
}

} // namespace

std::optional<UUID> PickSpriteEntity(
    Scene& scene,
    Vector2 worldPoint)
{
    std::optional<UUID> selected;
    i32 selectedLayer = std::numeric_limits<i32>::min();

    // Scene::GetEntities is UUID-sorted, so replacing a same-layer hit with
    // the later candidate provides a stable deterministic fallback order.
    for (const ECS::Entity entity : scene.GetEntities())
    {
        const auto* identity =
            scene.GetComponent<EntityIdentityComponent>(entity);
        const auto* transform =
            scene.GetComponent<TransformComponent>(entity);
        const auto* sprite =
            scene.GetComponent<SpriteRendererComponent>(entity);

        if (identity == nullptr
            || transform == nullptr
            || sprite == nullptr
            || !sprite->enabled
            || !sprite->texture.IsValid())
        {
            continue;
        }

        if (!ContainsPoint(
                *transform,
                *sprite,
                worldPoint))
        {
            continue;
        }

        if (!selected.has_value()
            || sprite->layer >= selectedLayer)
        {
            selected = identity->id;
            selectedLayer = sprite->layer;
        }
    }

    return selected;
}

std::optional<UUID> PickSpriteEntity(
    Scene& scene,
    const EditorCamera& camera,
    Viewport viewport,
    Vector2 viewportPoint)
{
    return PickSpriteEntity(
        scene,
        camera.ScreenToWorld(
            viewportPoint,
            viewport));
}

} // namespace Janus::Editor
