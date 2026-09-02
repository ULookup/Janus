#include "ECS/SparseSet.h"

namespace Janus::ECS
{

bool SparseSet::Contains(Entity entity) const noexcept
{
    return entity.IsValid()
        && entity.index < m_Sparse.size()
        && m_Sparse[entity.index] == entity;
}

u32 SparseSet::DenseIndex(Entity entity) const noexcept
{
    return Contains(entity)
        ? m_SparseToDense[entity.index]
        : InvalidDenseIndex;
}

const std::vector<Entity>& SparseSet::Entities() const noexcept
{
    return m_Dense;
}

bool SparseSet::Add(Entity entity)
{
    if (!entity.IsValid() || Contains(entity))
    {
        return false;
    }

    if (entity.index >= m_Sparse.size())
    {
        m_Sparse.resize(entity.index + 1, Entity{});
        m_SparseToDense.resize(
            entity.index + 1,
            InvalidDenseIndex);
    }

    const u32 denseIndex = static_cast<u32>(m_Dense.size());
    m_Sparse[entity.index] = entity;
    m_SparseToDense[entity.index] = denseIndex;
    m_Dense.push_back(entity);
    return true;
}

void SparseSet::Remove(Entity entity)
{
    if (!Contains(entity))
    {
        return;
    }

    const u32 removedIndex = m_SparseToDense[entity.index];
    const Entity moved = m_Dense.back();

    m_Dense[removedIndex] = moved;
    m_SparseToDense[moved.index] = removedIndex;

    m_Sparse[entity.index] = Entity{};
    m_SparseToDense[entity.index] = InvalidDenseIndex;
    m_Dense.pop_back();
}

} // namespace Janus::ECS
