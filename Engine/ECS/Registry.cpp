#include "ECS/Registry.h"

namespace Janus::ECS
{

Entity Registry::CreateEntity()
{
    if (!m_FreeIndices.empty())
    {
        const u32 index = m_FreeIndices.back();
        m_FreeIndices.pop_back();
        ++m_AliveCount;
        return Entity{index, m_Generations[index]};
    }

    const u32 index = static_cast<u32>(m_Generations.size());
    m_Generations.push_back(1);
    ++m_AliveCount;
    return Entity{index, 1};
}

bool Registry::DestroyEntity(Entity entity)
{
    if (!IsValid(entity))
    {
        return false;
    }

    for (auto& [type, pool] : m_Pools)
    {
        (void)type;
        pool->Remove(entity);
    }

    ++m_Generations[entity.index];
    m_FreeIndices.push_back(entity.index);
    --m_AliveCount;
    return true;
}

bool Registry::IsValid(Entity entity) const noexcept
{
    return entity.IsValid()
        && entity.index < m_Generations.size()
        && m_Generations[entity.index] == entity.generation;
}

usize Registry::AliveEntityCount() const noexcept
{
    return m_AliveCount;
}

} // namespace Janus::ECS
