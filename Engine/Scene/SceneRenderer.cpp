#include "Scene/SceneRenderer.h"

#include "Asset/AssetService.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Sprite.h"
#include "Scene/Components.h"
#include "Scene/Hierarchy.h"
#include "Scene/Scene.h"

namespace Janus
{

Result<void> SceneRenderer::Render(
    Scene& scene,
    AssetService& assets,
    Renderer2D& renderer,
    Viewport viewport)
{
    UpdateTransforms(scene);

    const auto cameraResult = FindCamera(scene);
    if (!cameraResult)
    {
        return Result<void>::Failure(cameraResult.GetError());
    }

    const ECS::Entity cameraEntity = cameraResult.Value();
    const auto* cameraTransform =
        scene.GetComponent<TransformComponent>(cameraEntity);
    const auto* cameraComponent =
        scene.GetComponent<CameraComponent>(cameraEntity);

    OrthographicCamera camera;
    camera.position = cameraTransform->worldPosition;
    camera.rotationRadians = cameraTransform->worldRotationRadians;
    camera.zoom = cameraComponent->zoom;

    renderer.SetViewport(viewport);
    renderer.BeginFrame(camera);

    for (const ECS::Entity entity : scene.GetEntities())
    {
        auto* transform = scene.GetComponent<TransformComponent>(entity);
        auto* spriteComponent =
            scene.GetComponent<SpriteRendererComponent>(entity);

        if (transform == nullptr
            || spriteComponent == nullptr
            || !spriteComponent->enabled
            || !spriteComponent->texture.IsValid())
        {
            continue;
        }

        auto textureResult = assets.LoadTexture(spriteComponent->texture);
        if (!textureResult)
        {
            const auto* identity =
                scene.GetComponent<EntityIdentityComponent>(entity);
            const std::string entityLabel = identity == nullptr
                ? std::string{"unknown entity"}
                : identity->name + " (" + identity->id.ToString() + ")";

            return Result<void>::Failure(
                textureResult.GetError().code,
                "Failed to resolve sprite texture for "
                    + entityLabel + ": "
                    + textureResult.GetError().message);
        }

        Sprite sprite;
        sprite.texture = textureResult.Value();
        sprite.position = transform->worldPosition;
        sprite.size = Vector2{
            spriteComponent->size.x * transform->worldScale.x,
            spriteComponent->size.y * transform->worldScale.y};
        sprite.rotationRadians = transform->worldRotationRadians;
        sprite.color = spriteComponent->color;
        sprite.layer = spriteComponent->layer;
        sprite.uv = spriteComponent->uv;
        sprite.blendMode = BlendMode::Alpha;

        renderer.SubmitSprite(sprite);
    }

    return renderer.EndFrame();
}

void SceneRenderer::UpdateTransforms(Scene& scene)
{
    for (const ECS::Entity entity : scene.GetEntities())
    {
        const auto* hierarchy =
            scene.GetComponent<HierarchyComponent>(entity);
        if (hierarchy != nullptr && !hierarchy->parent.IsValid())
        {
            UpdateTransformRecursive(
                scene,
                entity,
                Mat4::Identity(),
                0.0f,
                Vector2{1.0f, 1.0f});
        }
    }
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

    for (const ECS::Entity entity : scene.GetEntities())
    {
        if (scene.GetComponent<TransformComponent>(entity) == nullptr)
        {
            continue;
        }

        const auto* camera = scene.GetComponent<CameraComponent>(entity);
        if (camera == nullptr)
        {
            continue;
        }

        if (!firstCamera.IsValid())
        {
            firstCamera = entity;
        }

        if (camera->primary)
        {
            return Result<ECS::Entity>::Success(entity);
        }
    }

    if (!firstCamera.IsValid())
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::CameraNotFound,
            "Scene has no camera.");
    }

    return Result<ECS::Entity>::Success(firstCamera);
}

} // namespace Janus
