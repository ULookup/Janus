#pragma once

#include "Core/Types.h"
#include "ECS/ComponentPool.h"
#include "ECS/Entity.h"

#include <functional>
#include <tuple>
#include <utility>

namespace Janus::ECS
{

template <typename... Components>
class View
{
public:
    template <typename Function>
    void ForEach(Function&& function) const
    {
        ForEachImpl(
            std::forward<Function>(function),
            std::index_sequence_for<Components...>{});
    }

private:
    explicit View(
        std::tuple<ComponentPool<Components>*...> pools)
        : m_Pools(std::move(pools))
    {
        m_PrimaryEntities =
            &std::get<0>(m_Pools)->Entities();
    }

    template <usize... Is>
    bool HasAll(
        Entity entity,
        std::index_sequence<Is...>) const
    {
        return (std::get<Is>(m_Pools)->Has(entity) && ...);
    }

    template <typename Function, usize... Is>
    void ForEachImpl(
        Function&& function,
        std::index_sequence<Is...>) const
    {
        for (Entity entity : *m_PrimaryEntities)
        {
            if (!HasAll(entity, std::index_sequence<Is...>{}))
            {
                continue;
            }

            std::invoke(
                std::forward<Function>(function),
                entity,
                *std::get<Is>(m_Pools)->Get(entity)...);
        }
    }

    std::tuple<ComponentPool<Components>*...> m_Pools;
    const std::vector<Entity>* m_PrimaryEntities = nullptr;

    friend class Registry;
};

} // namespace Janus::ECS
