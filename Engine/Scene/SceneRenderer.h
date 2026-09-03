#pragma once

#include "Core/Error/Result.h"
#include "Core/Math/Mat4.h"
#include "Core/Math/Vector2.h"
#include "ECS/Entity.h"
#include "Renderer/RendererTypes.h"

namespace Janus
{

class AssetService;
class Renderer2D;
class Scene;

class SceneRenderer
{
public:
    [[nodiscard]] Result<void> Render(
        Scene& scene,
        AssetService& assets,
        Renderer2D& renderer,
        Viewport viewport);

private:
    void UpdateTransforms(Scene& scene);
    void UpdateTransformRecursive(
        Scene& scene,
        ECS::Entity entity,
        const Mat4& parentWorld,
        f32 parentWorldRotation,
        Vector2 parentWorldScale);

    [[nodiscard]] Result<ECS::Entity> FindCamera(Scene& scene) const;
};

} // namespace Janus
