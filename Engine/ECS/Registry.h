#pragma once

#include "Core/Types.h"
#include "ECS/ComponentPool.h"
#include "ECS/Entity.h"
#include "ECS/View.h"

#include <memory>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Janus::ECS
{

class Registry
{
public:
    [[nodiscard]] Entity CreateEntity();
    bool DestroyEntity(Entity entity);
    [[nodiscard]] bool IsValid(Entity entity) const noexcept;
    [[nodiscard]] usize AliveEntityCount() const noexcept;

    template <typename T>
    T* AddComponent(Entity entity, T component)
    {
        return GetPool<T>().Add(entity, std::move(component));
    }

    template <typename T>
    bool RemoveComponent(Entity entity)
    {
        auto* pool = FindPool<T>();
        if (pool == nullptr || !pool->Has(entity))
        {
            return false;
        }

        pool->Remove(entity);
        return true;
    }

    template <typename T>
    [[nodiscard]] T* GetComponent(Entity entity) noexcept
    {
        auto* pool = FindPool<T>();
        return pool == nullptr ? nullptr : pool->Get(entity);
    }

    template <typename T>
    [[nodiscard]] const T* GetComponent(Entity entity) const noexcept
    {
        const auto* pool = FindPool<T>();
        return pool == nullptr ? nullptr : pool->Get(entity);
    }

    template <typename T>
    [[nodiscard]] bool HasComponent(Entity entity) const noexcept
    {
        const auto* pool = FindPool<T>();
        return pool != nullptr && pool->Has(entity);
    }

    template <typename... Components>
    [[nodiscard]] View<Components...> View()
    {
        return ::Janus::ECS::View<Components...>(
            std::make_tuple(&GetPool<Components>()...));
    }

private:
    template <typename T>
    ComponentPool<T>* FindPool() noexcept
    {
        const auto it =
            m_Pools.find(std::type_index(typeid(T)));
        return it == m_Pools.end()
            ? nullptr
            : static_cast<ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    const ComponentPool<T>* FindPool() const noexcept
    {
        const auto it =
            m_Pools.find(std::type_index(typeid(T)));
        return it == m_Pools.end()
            ? nullptr
            : static_cast<const ComponentPool<T>*>(
                it->second.get());
    }

    template <typename T>
    ComponentPool<T>& GetPool()
    {
        auto& raw = m_Pools[std::type_index(typeid(T))];
        if (!raw)
        {
            raw = std::make_unique<ComponentPool<T>>();
        }

        return *static_cast<ComponentPool<T>*>(raw.get());
    }

    std::vector<u32> m_Generations;
    std::vector<u32> m_FreeIndices;
    std::unordered_map<
        std::type_index,
        std::unique_ptr<IComponentPool>> m_Pools;
    usize m_AliveCount = 0;
};

} // namespace Janus::ECS
