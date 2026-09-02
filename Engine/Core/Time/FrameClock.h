#pragma once

#include "Core/Time/TimeStep.h"

#include <chrono>
#include <optional>

namespace Janus
{

class FrameClock final
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    [[nodiscard]] TimeStep Tick() noexcept;
    [[nodiscard]] TimeStep Tick(TimePoint now) noexcept;

private:
    std::optional<TimePoint> m_Previous;
};

} // namespace Janus
