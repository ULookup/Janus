#pragma once

#include "Core/Types.h"

#include <algorithm>

namespace Janus
{

class TimeStep final
{
public:
    constexpr TimeStep() noexcept = default;

    [[nodiscard]]
    static constexpr TimeStep FromSeconds(f64 seconds) noexcept
    {
        return TimeStep(seconds);
    }

    [[nodiscard]]
    static constexpr TimeStep FromMilliseconds(f64 milliseconds) noexcept
    {
        return TimeStep(milliseconds / 1000.0);
    }

    [[nodiscard]]
    constexpr f64 GetSeconds() const noexcept
    {
        return m_Seconds;
    }

    [[nodiscard]]
    constexpr f64 GetMilliseconds() const noexcept
    {
        return m_Seconds * 1000.0;
    }

    [[nodiscard]]
    constexpr TimeStep ClampedTo(TimeStep maximum) const noexcept
    {
        return TimeStep(std::min(m_Seconds, maximum.m_Seconds));
    }

private:
    explicit constexpr TimeStep(f64 seconds) noexcept
        : m_Seconds(std::max(seconds, 0.0))
    {
    }

    f64 m_Seconds = 0.0;
};

} // namespace Janus
