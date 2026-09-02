#pragma once

#include "Core/Types.h"
#include "ECS/Entity.h"

#include <vector>

namespace Janus::ECS
{

class SparseSet
{
public:
    [[nodiscard]] bool Contains(Entity entity) const noexcept;
    [[nodiscard]] u32 DenseIndex(Entity entity) const noexcept;
    [[nodiscard]] const std::vector<Entity>& Entities() const noexcept;

    void Add(Entity entity);
    void Remove(Entity entity);

private:
    static constexpr u32 InvalidDenseIndex = 0xFFFFFFFFu;

    std::vector<Entity> m_Sparse;
    std::vector<u32> m_SparseToDense;
    std::vector<Entity> m_Dense;
};

} // namespace Janus::ECS
