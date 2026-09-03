#include "Scene/Scene.h"

#include "Core/Math/Mat4.h"
#include "Core/Math/Vector2.h"
#include "ECS/View.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Sprite.h"

#include <vector>

namespace Janus
{

Scene::Scene() = default;
Scene::~Scene() = default;

ECS::Entity Scene::CreateEntity()
{
    const ECS::Entity entity = m_Registry.CreateEntity();
    m_Registry.AddComponent<TransformComponent>(
        entity,
        TransformComponent{});
    m_Registry.AddComponent<HierarchyComponent>(
        entity,
        HierarchyComponent{});

    return entity;
}

bool Scene::DestroyEntity(ECS::Entity entity)
{
    if (!m_Registry.IsValid(entity))
    {
        return false;
    }

    DestroyEntityRecursive(entity);
    return true;
}

Result<void> Scene::SetParent(
    ECS::Entity child,
    ECS::Entity parent)
{
    if (!m_Registry.IsValid(child))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidEntity,
            "Cannot reparent an invalid child.");
    }

    if (parent.IsValid() && !m_Registry.IsValid(parent))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidEntity,
            "Cannot reparent to an invalid parent.");
    }

    if (!m_Registry.HasComponent<HierarchyComponent>(child)
        || (parent.IsValid()
            && !m_Registry.HasComponent<HierarchyComponent>(parent)))
    {
        return Result<void>::Failure(
            ErrorCode::EntityNotFound,
            "Scene hierarchy components are required.");
    }

    if (child == parent)
    {
        return Result<void>::Failure(
            ErrorCode::HierarchyCycle,
            "An entity cannot be its own parent.");
    }

    if (parent.IsValid() && IsAncestor(parent, child))
    {
        return Result<void>::Failure(
            ErrorCode::HierarchyCycle,
            "Cannot create a hierarchy cycle.");
    }

    DetachFromParent(child);

    auto* childHierarchy =
        m_Registry.GetComponent<HierarchyComponent>(child);
    childHierarchy->parent = parent;

    if (parent.IsValid())
    {
        auto* parentHierarchy =
            m_Registry.GetComponent<HierarchyComponent>(parent);

        childHierarchy->nextSibling =
            parentHierarchy->firstChild;

        if (parentHierarchy->firstChild.IsValid())
        {
            auto* oldFirst =
                m_Registry.GetComponent<HierarchyComponent>(
                    parentHierarchy->firstChild);
            oldFirst->previousSibling = child;
        }

        parentHierarchy->firstChild = child;
    }

    auto* transform =
        m_Registry.GetComponent<TransformComponent>(child);
    transform->dirty = true;

    return Result<void>::Success();
}

const ECS::Registry& Scene::GetRegistry() const noexcept
{
    return m_Registry;
}

void Scene::UpdateTransforms()
{
    m_Registry
        .View<TransformComponent, HierarchyComponent>()
        .ForEach(
            [this](
                ECS::Entity entity,
                TransformComponent&,
                HierarchyComponent& hierarchy)
            {
                if (!hierarchy.parent.IsValid())
                {
                    UpdateTransformRecursive(
                        entity,
                        Mat4::Identity(),
                        0.0f,
                        Vector2{1.0f, 1.0f});
                }
            });
}

void Scene::UpdateTransformRecursive(
    ECS::Entity entity,
    const Mat4& parentWorld,
    f32 parentWorldRotation,
    Vector2 parentWorldScale)
{
    auto* transform =
        m_Registry.GetComponent<TransformComponent>(entity);
    auto* hierarchy =
        m_Registry.GetComponent<HierarchyComponent>(entity);

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
        auto* childHierarchy =
            m_Registry.GetComponent<HierarchyComponent>(child);
        UpdateTransformRecursive(
            child,
            world,
            transform->worldRotationRadians,
            transform->worldScale);
        child = childHierarchy->nextSibling;
    }
}

Result<ECS::Entity> Scene::FindCamera()
{
    ECS::Entity firstCamera = ECS::Entity{};
    bool primaryFound = false;

    m_Registry
        .View<TransformComponent, CameraComponent>()
        .ForEach(
            [&](
                ECS::Entity entity,
                TransformComponent&,
                CameraComponent& camera)
            {
                if (!firstCamera.IsValid())
                {
                    firstCamera = entity;
                }

                if (camera.primary && !primaryFound)
                {
                    firstCamera = entity;
                    primaryFound = true;
                }
            });

    if (!firstCamera.IsValid())
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::CameraNotFound,
            "Scene has no camera.");
    }

    return Result<ECS::Entity>::Success(firstCamera);
}

Result<void> Scene::Render(
    Renderer2D& renderer,
    Viewport viewport)
{
    const auto cameraResult = FindCamera();
    if (!cameraResult)
    {
        return Result<void>::Failure(cameraResult.GetError());
    }

    UpdateTransforms();

    const ECS::Entity cameraEntity = cameraResult.Value();
    const auto* cameraTransform =
        m_Registry.GetComponent<TransformComponent>(cameraEntity);
    const auto* cameraComponent =
        m_Registry.GetComponent<CameraComponent>(cameraEntity);

    OrthographicCamera camera;
    camera.position = cameraTransform->worldPosition;
    camera.rotationRadians =
        cameraTransform->worldRotationRadians;
    camera.zoom = cameraComponent->zoom;

    renderer.SetViewport(viewport);
    renderer.BeginFrame(camera);

    m_Registry
        .View<TransformComponent, SpriteRendererComponent>()
        .ForEach(
            [&](
                ECS::Entity,
                TransformComponent& transform,
                SpriteRendererComponent& spriteComponent)
            {
                if (!spriteComponent.enabled
                    || spriteComponent.texture.value == 0)
                {
                    return;
                }

                Sprite sprite;
                sprite.texture = spriteComponent.texture;
                sprite.position = transform.worldPosition;
                sprite.size = Vector2{
                    spriteComponent.size.x
                        * transform.worldScale.x,
                    spriteComponent.size.y
                        * transform.worldScale.y};
                sprite.rotationRadians =
                    transform.worldRotationRadians;
                sprite.color = spriteComponent.color;
                sprite.layer = spriteComponent.layer;
                sprite.uv = spriteComponent.uv;
                sprite.blendMode = BlendMode::Alpha;

                renderer.SubmitSprite(sprite);
            });

    return renderer.EndFrame();
}

void Scene::DetachFromParent(ECS::Entity entity)
{
    auto* hierarchy =
        m_Registry.GetComponent<HierarchyComponent>(entity);

    if (hierarchy == nullptr)
    {
        return;
    }

    const ECS::Entity parent = hierarchy->parent;

    if (!parent.IsValid())
    {
        return;
    }

    auto* parentHierarchy =
        m_Registry.GetComponent<HierarchyComponent>(parent);

    if (parentHierarchy->firstChild == entity)
    {
        parentHierarchy->firstChild = hierarchy->nextSibling;
    }

    if (hierarchy->previousSibling.IsValid())
    {
        auto* previous =
            m_Registry.GetComponent<HierarchyComponent>(
                hierarchy->previousSibling);
        previous->nextSibling = hierarchy->nextSibling;
    }

    if (hierarchy->nextSibling.IsValid())
    {
        auto* next =
            m_Registry.GetComponent<HierarchyComponent>(
                hierarchy->nextSibling);
        next->previousSibling = hierarchy->previousSibling;
    }

    hierarchy->parent = ECS::Entity{};
    hierarchy->previousSibling = ECS::Entity{};
    hierarchy->nextSibling = ECS::Entity{};
}

void Scene::DestroyEntityRecursive(ECS::Entity entity)
{
    auto* hierarchy =
        m_Registry.GetComponent<HierarchyComponent>(entity);

    if (hierarchy == nullptr)
    {
        m_Registry.DestroyEntity(entity);
        return;
    }

    std::vector<ECS::Entity> children;
    ECS::Entity child = hierarchy->firstChild;
    while (child.IsValid())
    {
        children.push_back(child);
        auto* childHierarchy =
            m_Registry.GetComponent<HierarchyComponent>(child);
        child = childHierarchy->nextSibling;
    }

    for (ECS::Entity childEntity : children)
    {
        DestroyEntityRecursive(childEntity);
    }

    DetachFromParent(entity);
    m_Registry.DestroyEntity(entity);
}

bool Scene::IsAncestor(
    ECS::Entity entity,
    ECS::Entity possibleAncestor) const
{
    ECS::Entity current = entity;

    while (current.IsValid())
    {
        const auto* hierarchy =
            m_Registry.GetComponent<HierarchyComponent>(current);

        if (hierarchy == nullptr)
        {
            return false;
        }

        if (hierarchy->parent == possibleAncestor)
        {
            return true;
        }

        current = hierarchy->parent;
    }

    return false;
}

} // namespace Janus
