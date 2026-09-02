#include "Core/Time/FrameClock.h"

namespace Janus
{

TimeStep FrameClock::Tick() noexcept
{
    return Tick(Clock::now());
}

TimeStep FrameClock::Tick(TimePoint now) noexcept
{
    if (!m_Previous)
    {
        m_Previous = now;
        return TimeStep{};
    }

    const auto elapsed = std::chrono::duration<f64>(now - *m_Previous).count();
    m_Previous = now;
    return TimeStep::FromSeconds(elapsed);
}

} // namespace Janus
