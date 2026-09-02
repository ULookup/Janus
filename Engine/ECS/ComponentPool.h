#pragma once

#include "ECS/Entity.h"
#include "ECS/SparseSet.h"

#include <utility>
#include <vector>

namespace Janus::ECS
{

class IComponentPool
{
public:
    virtual ~IComponentPool() = default;
    virtual void Remove(Entity entity) = 0;
};

template <typename T>
class ComponentPool final : public IComponentPool
{
public:
    [[nodiscard]] bool Has(Entity entity) const noexcept
    {
        return m_Sparse.Contains(entity);
    }

    [[nodiscard]] T* Get(Entity entity) noexcept
    {
        if (!Has(entity))
        {
            return nullptr;
        }

        return &m_Components[m_Sparse.DenseIndex(entity)];
    }

    [[nodiscard]] const T* Get(Entity entity) const noexcept
    {
        if (!Has(entity))
        {
            return nullptr;
        }

        return &m_Components[m_Sparse.DenseIndex(entity)];
    }

    T& Add(Entity entity, T component)
    {
        if (Has(entity))
        {
            m_Components[m_Sparse.DenseIndex(entity)] =
                std::move(component);
            return m_Components[m_Sparse.DenseIndex(entity)];
        }

        m_Sparse.Add(entity);
        m_Components.push_back(std::move(component));
        return m_Components.back();
    }

    void Remove(Entity entity) override
    {
        if (!Has(entity))
        {
            return;
        }

        const u32 removedIndex = m_Sparse.DenseIndex(entity);
        m_Components[removedIndex] = std::move(m_Components.back());
        m_Components.pop_back();
        m_Sparse.Remove(entity);
    }

    [[nodiscard]] const std::vector<Entity>& Entities() const noexcept
    {
        return m_Sparse.Entities();
    }

private:
    SparseSet m_Sparse;
    std::vector<T> m_Components;
};

} // namespace Janus::ECS
