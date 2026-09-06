#include "Core/Log/Log.h"
#include "Core/Log/LogStore.h"

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

TEST_CASE("Structured log sink is shared and detached on shutdown", "[log-store][v0.9]")
{
    using namespace Janus;
    Log::Shutdown();
    auto store = std::make_shared<LogStore>();
    std::weak_ptr<LogStore> weak = store;
    Log::Initialize(LogOutput::StandardError, store);
    JANUS_CORE_WARN("same store warning");
    JANUS_ERROR("same store error");
    auto page = store->Read();
    REQUIRE(page);
    REQUIRE(page.Value().entries.size() == 3);
    REQUIRE(page.Value().entries[1].level == LogLevel::Warning);
    REQUIRE(page.Value().entries[2].level == LogLevel::Error);
    store.reset();
    REQUIRE_FALSE(weak.expired());
    Log::Shutdown();
    REQUIRE(weak.expired());
}
