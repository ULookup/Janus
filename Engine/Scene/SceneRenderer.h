#pragma once

#include "Core/Error/Result.h"
#include "Core/Math/Mat4.h"
#include "Core/Math/Vector2.h"
#include "ECS/Entity.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/RendererTypes.h"

namespace Janus
{

class AssetService;
class Renderer2D;
class Scene;
class AnimationSystem;
struct UIInteractionState;

struct SceneRenderRequest
{
    Scene& scene;
    AssetService& assets;
    Renderer2D& renderer;
    OrthographicCamera camera;
    Viewport viewport;
    RenderTargetHandle target;
    // Game world projection and screen-space UI share this logical size; zero uses target pixels.
    Viewport logicalViewport;
    bool includeUI = true;
    const UIInteractionState* uiState = nullptr;
    const AnimationSystem* animations = nullptr;
};

class SceneRenderer
{
public:
  [[nodiscard]] Result<void> Render(Scene& scene, AssetService& assets, Renderer2D& renderer,
                                    Viewport viewport, Viewport logicalViewport = {},
                                    const UIInteractionState* uiState = nullptr,
                                    const AnimationSystem* animations = nullptr);

  [[nodiscard]] Result<void> Render(const SceneRenderRequest& request);

  [[nodiscard]] Result<OrthographicCamera> ResolvePrimaryCamera(Scene& scene);

private:
    [[nodiscard]] Result<void> RenderPrepared(
        const SceneRenderRequest& request);

    void UpdateTransforms(Scene& scene);
    void UpdateTransformRecursive(
        Scene& scene,
        ECS::Entity entity,
        const Mat4& parentWorld,
        f32 parentWorldRotation,
        Vector2 parentWorldScale);

    [[nodiscard]] Result<ECS::Entity> FindCamera(Scene& scene) const;
    [[nodiscard]] OrthographicCamera BuildCamera(
        Scene& scene,
        ECS::Entity entity) const;
};

} // namespace Janus
