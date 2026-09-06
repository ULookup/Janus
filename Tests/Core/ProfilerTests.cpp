#include "Core/Profiling/CpuProfiler.h"
#include <catch2/catch_test_macros.hpp>
#include <thread>
TEST_CASE("CPU profiler publishes completed bounded frames with nested scopes", "[profiler][v0.9]")
{
    using namespace Janus;
    double clock = 0;
    CpuProfiler profiler([&]() { return clock; }, 2, 2);
    REQUIRE(profiler.BeginFrame());
    REQUIRE_FALSE(profiler.Latest());
    {
        CpuScope outer(profiler, "outer");
        clock = 1;
        {
            CpuScope inner(profiler, "inner");
            clock = 3;
        }
        clock = 5;
    }
    REQUIRE(profiler.EndFrame());
    const auto frame = profiler.Latest();
    REQUIRE(frame);
    REQUIRE(frame->durationMilliseconds == 5);
    REQUIRE(frame->scopes.size() == 2);
    REQUIRE(frame->scopes[1].parent == 0);
    REQUIRE(frame->scopes[0].durationMilliseconds == 5);
    REQUIRE(frame->scopes[1].durationMilliseconds == 2);
    REQUIRE(profiler.BeginFrame());
    {
        CpuScope a(profiler, "a");
        CpuScope b(profiler, "b");
        CpuScope c(profiler, "c");
    }
    REQUIRE(profiler.EndFrame());
    REQUIRE(profiler.Latest()->droppedScopes == 1);
    REQUIRE(profiler.BeginFrame());
    REQUIRE(profiler.EndFrame());
    REQUIRE_FALSE(profiler.Find(1));
    profiler.SetEnabled(false);
    REQUIRE_FALSE(profiler.BeginFrame());
}
TEST_CASE("CPU profiler rejects foreign threads and never publishes unclosed scopes",
          "[profiler][v0.9]")
{
    Janus::CpuProfiler profiler;
    bool foreign = true;
    std::thread worker([&] { foreign = profiler.BeginFrame(); });
    worker.join();
    REQUIRE_FALSE(foreign);
    REQUIRE(profiler.BeginFrame());
    {
        Janus::CpuScope scope(profiler, "open");
        REQUIRE_FALSE(profiler.EndFrame());
    }
    REQUIRE(profiler.EndFrame());
    REQUIRE(profiler.Latest()->scopes.size() == 1);
}
