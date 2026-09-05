#include "Scene/SceneRenderer.h"

#include "Asset/AssetService.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Sprite.h"
#include "Scene/Components.h"
#include "Scene/Hierarchy.h"
#include "Scene/Scene.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Janus
{

Result<void> SceneRenderer::Render(
    Scene& scene,
    AssetService& assets,
    Renderer2D& renderer,
    Viewport viewport)
{
    UpdateTransforms(scene);

    const auto cameraEntity = FindCamera(scene);
    if (!cameraEntity)
    {
        return Result<void>::Failure(
            cameraEntity.GetError());
    }

    return RenderPrepared(
        SceneRenderRequest{
            scene,
            assets,
            renderer,
            BuildCamera(scene, cameraEntity.Value()),
            viewport,
            {}});
}

Result<void> SceneRenderer::Render(
    const SceneRenderRequest& request)
{
    UpdateTransforms(request.scene);
    return RenderPrepared(request);
}

Result<OrthographicCamera> SceneRenderer::ResolvePrimaryCamera(
    Scene& scene)
{
    UpdateTransforms(scene);

    const auto cameraEntity = FindCamera(scene);
    if (!cameraEntity)
    {
        return Result<OrthographicCamera>::Failure(
            cameraEntity.GetError());
    }

    return Result<OrthographicCamera>::Success(
        BuildCamera(scene, cameraEntity.Value()));
}

Result<void> SceneRenderer::RenderPrepared(
    const SceneRenderRequest& request)
{
    std::vector<Sprite> sprites;
    std::optional<Error> extractionError;

    request.scene.View<TransformComponent, SpriteRendererComponent>()
        .ForEach(
            [&](ECS::Entity entity,
                TransformComponent& transform,
                SpriteRendererComponent& spriteComponent)
            {
                if (extractionError.has_value()
                    || !spriteComponent.enabled
                    || !spriteComponent.texture.IsValid())
                {
                    return;
                }

                auto textureResult =
                    request.assets.LoadTexture(spriteComponent.texture);
                if (!textureResult)
                {
                    const auto* identity =
                        request.scene.GetComponent<EntityIdentityComponent>(
                            entity);
                    const std::string entityLabel = identity == nullptr
                        ? std::string{"unknown entity"}
                        : identity->name + " (" + identity->id.ToString() + ")";

                    extractionError = Error{
                        textureResult.GetError().code,
                        "Failed to resolve sprite texture for "
                            + entityLabel + ": "
                            + textureResult.GetError().message};
                    return;
                }

                Sprite sprite;
                sprite.texture = textureResult.Value();
                sprite.position = transform.worldPosition;
                sprite.size = Vector2{
                    spriteComponent.size.x * transform.worldScale.x,
                    spriteComponent.size.y * transform.worldScale.y};
                sprite.rotationRadians = transform.worldRotationRadians;
                sprite.color = spriteComponent.color;
                sprite.layer = spriteComponent.layer;
                sprite.uv = spriteComponent.uv;
                sprite.blendMode = BlendMode::Alpha;

                sprites.push_back(sprite);
            });

    if (extractionError.has_value())
    {
        return Result<void>::Failure(
            std::move(*extractionError));
    }

    RenderFrameDesc frame;
    frame.camera = request.camera;
    frame.viewport = request.viewport;
    frame.target = request.target;

    auto began = request.renderer.BeginFrame(frame);
    if (!began)
    {
        return began;
    }

    for (const Sprite& sprite : sprites)
    {
        request.renderer.SubmitSprite(sprite);
    }

    return request.renderer.EndFrame();
}

void SceneRenderer::UpdateTransforms(Scene& scene)
{
    scene.View<TransformComponent, HierarchyComponent>()
        .ForEach(
            [this, &scene](
                ECS::Entity entity,
                TransformComponent&,
                HierarchyComponent& hierarchy)
            {
                if (!hierarchy.parent.IsValid())
                {
                    UpdateTransformRecursive(
                        scene,
                        entity,
                        Mat4::Identity(),
                        0.0f,
                        Vector2{1.0f, 1.0f});
                }
            });
}

void SceneRenderer::UpdateTransformRecursive(
    Scene& scene,
    ECS::Entity entity,
    const Mat4& parentWorld,
    f32 parentWorldRotation,
    Vector2 parentWorldScale)
{
    auto* transform = scene.GetComponent<TransformComponent>(entity);
    auto* hierarchy = scene.GetComponent<HierarchyComponent>(entity);

    if (transform == nullptr || hierarchy == nullptr)
    {
        return;
    }

    const Mat4 local = Mat4::Multiply(
        Mat4::Translate(transform->position),
        Mat4::Multiply(
            Mat4::Rotate(transform->rotationRadians),
            Mat4::Scale(transform->scale)));

    const Mat4 world = Mat4::Multiply(parentWorld, local);

    transform->worldPosition =
        Mat4::TransformPoint(parentWorld, transform->position);
    transform->worldRotationRadians =
        parentWorldRotation + transform->rotationRadians;
    transform->worldScale = Vector2{
        parentWorldScale.x * transform->scale.x,
        parentWorldScale.y * transform->scale.y};
    transform->dirty = false;

    ECS::Entity child = hierarchy->firstChild;
    while (child.IsValid())
    {
        const auto* childHierarchy =
            scene.GetComponent<HierarchyComponent>(child);
        if (childHierarchy == nullptr)
        {
            break;
        }

        const ECS::Entity nextSibling = childHierarchy->nextSibling;
        UpdateTransformRecursive(
            scene,
            child,
            world,
            transform->worldRotationRadians,
            transform->worldScale);
        child = nextSibling;
    }
}

Result<ECS::Entity> SceneRenderer::FindCamera(Scene& scene) const
{
    ECS::Entity firstCamera;
    ECS::Entity primaryCamera;

    scene.View<TransformComponent, CameraComponent>()
        .ForEach(
            [&](ECS::Entity entity,
                TransformComponent&,
                CameraComponent& camera)
            {
                if (!firstCamera.IsValid())
                {
                    firstCamera = entity;
                }

                if (camera.primary && !primaryCamera.IsValid())
                {
                    primaryCamera = entity;
                }
            });

    if (primaryCamera.IsValid())
    {
        return Result<ECS::Entity>::Success(primaryCamera);
    }

    if (!firstCamera.IsValid())
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::CameraNotFound,
            "Scene has no camera.");
    }

    return Result<ECS::Entity>::Success(firstCamera);
}

OrthographicCamera SceneRenderer::BuildCamera(
    Scene& scene,
    ECS::Entity entity) const
{
    const auto* cameraTransform =
        scene.GetComponent<TransformComponent>(entity);
    const auto* cameraComponent =
        scene.GetComponent<CameraComponent>(entity);

    OrthographicCamera camera;
    camera.position = cameraTransform->worldPosition;
    camera.rotationRadians =
        cameraTransform->worldRotationRadians;
    camera.zoom = cameraComponent->zoom;
    return camera;
}

} // namespace Janus
