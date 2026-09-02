#pragma once

#include "Core/Error/Result.h"
#include "ECS/Registry.h"
#include "Scene/Components.h"
#include "Scene/Hierarchy.h"

#include <memory>
#include <utility>

namespace Janus
{

class Renderer2D;

class Scene
{
public:
    Scene();
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    [[nodiscard]] ECS::Entity CreateEntity();
    bool DestroyEntity(ECS::Entity entity);
    [[nodiscard]] Result<void> SetParent(
        ECS::Entity child,
        ECS::Entity parent);

    [[nodiscard]] const ECS::Registry& GetRegistry() const noexcept;

    template <typename T>
    T& AddComponent(ECS::Entity entity, T component)
    {
        return m_Registry.AddComponent<T>(
            entity,
            std::move(component));
    }

    template <typename T>
    [[nodiscard]] T* GetComponent(ECS::Entity entity) noexcept
    {
        return m_Registry.GetComponent<T>(entity);
    }

    template <typename T>
    [[nodiscard]] const T* GetComponent(
        ECS::Entity entity) const noexcept
    {
        return m_Registry.GetComponent<T>(entity);
    }

    template <typename T>
    [[nodiscard]] bool HasComponent(
        ECS::Entity entity) const noexcept
    {
        return m_Registry.HasComponent<T>(entity);
    }

    template <typename... Components>
    [[nodiscard]] ECS::View<Components...> View()
    {
        return m_Registry.View<Components...>();
    }

    [[nodiscard]] Result<void> Render(
        Renderer2D& renderer,
        Viewport viewport);

private:
    void UpdateTransforms();
    void UpdateTransformRecursive(
        ECS::Entity entity,
        const Mat4& parentWorld,
        f32 parentWorldRotation,
        Vector2 parentWorldScale,
        bool parentDirty);

    [[nodiscard]] Result<ECS::Entity> FindCamera();

    void DetachFromParent(ECS::Entity entity);
    void DestroyEntityRecursive(ECS::Entity entity);
    [[nodiscard]] bool IsAncestor(
        ECS::Entity entity,
        ECS::Entity possibleAncestor) const;

    ECS::Registry m_Registry;
};

} // namespace Janus
