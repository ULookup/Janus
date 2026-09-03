#pragma once

#include "Core/Error/Result.h"
#include "Core/UUID/UUID.h"
#include "ECS/Registry.h"
#include "Scene/Components.h"
#include "Scene/Hierarchy.h"
#include "Scene/SceneMetadata.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Janus
{

class Scene
{
public:
    Scene();
    explicit Scene(SceneMetadata metadata);
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    [[nodiscard]] ECS::Entity CreateEntity(std::string name = "Entity");
    [[nodiscard]] Result<ECS::Entity> CreateEntityWithUUID(
        UUID id,
        std::string name = "Entity");
    bool DestroyEntity(ECS::Entity entity);

    [[nodiscard]] Result<void> SetParent(
        ECS::Entity child,
        ECS::Entity parent);

    [[nodiscard]] ECS::Entity FindEntity(UUID id) const noexcept;
    [[nodiscard]] std::vector<ECS::Entity> GetEntities() const;
    [[nodiscard]] const SceneMetadata& GetMetadata() const noexcept;
    void SetName(std::string name);

    [[nodiscard]] const ECS::Registry& GetRegistry() const noexcept;

    template <typename T>
    T* AddComponent(ECS::Entity entity, T component)
    {
        return m_Registry.AddComponent<T>(
            entity,
            std::move(component));
    }

    template <typename T>
    bool RemoveComponent(ECS::Entity entity)
    {
        return m_Registry.RemoveComponent<T>(entity);
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

private:
    void DetachFromParent(ECS::Entity entity);
    void DestroyEntityRecursive(ECS::Entity entity);
    [[nodiscard]] bool IsAncestor(
        ECS::Entity entity,
        ECS::Entity possibleAncestor) const;

    SceneMetadata m_Metadata;
    ECS::Registry m_Registry;
    std::unordered_map<UUID, ECS::Entity, UUIDHash> m_EntityIndex;
};

} // namespace Janus
