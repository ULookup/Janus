#include "Core/Time/FrameClock.h"
#include "Core/Time/TimeStep.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>

TEST_CASE("TimeStep converts units and clamps", "[core][time]")
{
    const auto step = Janus::TimeStep::FromMilliseconds(500.0);

    REQUIRE(step.GetSeconds() == Catch::Approx(0.5));
    REQUIRE(step.GetMilliseconds() == Catch::Approx(500.0));
    REQUIRE(
        step.ClampedTo(Janus::TimeStep::FromMilliseconds(250.0))
            .GetMilliseconds()
        == Catch::Approx(250.0));
}

TEST_CASE("FrameClock returns zero first and elapsed time afterwards", "[core][time]")
{
    Janus::FrameClock clock;
    const Janus::FrameClock::TimePoint start{};

    REQUIRE(clock.Tick(start).GetSeconds() == Catch::Approx(0.0));
    REQUIRE(
        clock.Tick(start + std::chrono::milliseconds(16)).GetMilliseconds()
        == Catch::Approx(16.0));
}
