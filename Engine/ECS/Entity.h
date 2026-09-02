#pragma once

#include "Core/Types.h"

#include <compare>

namespace Janus::ECS
{

struct Entity
{
    static constexpr u32 InvalidIndex = 0xFFFFFFFFu;

    u32 index = InvalidIndex;
    u32 generation = 0;

    [[nodiscard]]
    bool IsValid() const noexcept
    {
        return index != InvalidIndex && generation != 0;
    }

    auto operator<=>(const Entity&) const = default;
};

} // namespace Janus::ECS
