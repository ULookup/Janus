#include "Core/Log/Log.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Logging macros are safe outside initialized lifetime", "[core][log]")
{
    Janus::Log::Shutdown();
    JANUS_CORE_INFO("safe before initialization");
    JANUS_INFO("safe before initialization");

    Janus::Log::Initialize();
    Janus::Log::Shutdown();
    JANUS_CORE_INFO("safe after shutdown");
    JANUS_INFO("safe after shutdown");

    SUCCEED();
}
