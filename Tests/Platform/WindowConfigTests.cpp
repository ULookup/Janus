#include "Platform/Window/WindowConfig.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>

TEST_CASE("WindowConfig rejects zero and oversized dimensions", "[platform][window]")
{
    Janus::WindowConfig config;
    config.width = 0;
    REQUIRE_FALSE(Janus::ValidateWindowConfig(config));

    config.width = 1280;
    config.height = 0;
    REQUIRE_FALSE(Janus::ValidateWindowConfig(config));

    config.width = static_cast<Janus::u32>(std::numeric_limits<Janus::i32>::max())
        + 1U;
    config.height = 720;
    REQUIRE_FALSE(Janus::ValidateWindowConfig(config));
}

TEST_CASE("WindowConfig accepts normal dimensions", "[platform][window]")
{
    Janus::WindowConfig config;
    config.width = 1280;
    config.height = 720;
    REQUIRE(Janus::ValidateWindowConfig(config));
}
