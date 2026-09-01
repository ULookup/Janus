#include "Core/Error/Result.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Result value success exposes its value", "[core][result]")
{
    auto result = Janus::Result<int>::Success(42);

    REQUIRE(result.HasValue());
    REQUIRE_FALSE(result.HasError());
    REQUIRE(static_cast<bool>(result));
    REQUIRE(result.Value() == 42);
}

TEST_CASE("Result value failure exposes its error", "[core][result]")
{
    auto result = Janus::Result<int>::Failure(
        Janus::ErrorCode::InvalidArgument,
        "invalid value");

    REQUIRE_FALSE(result.HasValue());
    REQUIRE(result.HasError());
    REQUIRE_FALSE(static_cast<bool>(result));
    REQUIRE(result.GetError().code == Janus::ErrorCode::InvalidArgument);
    REQUIRE(result.GetError().message == "invalid value");
}

TEST_CASE("Void result represents success", "[core][result]")
{
    auto result = Janus::Result<void>::Success();

    REQUIRE(result.HasValue());
    REQUIRE_FALSE(result.HasError());
    REQUIRE(static_cast<bool>(result));
}

TEST_CASE("Void result represents failure", "[core][result]")
{
    auto result = Janus::Result<void>::Failure(
        Janus::ErrorCode::PlatformInitFailed,
        "platform failed");

    REQUIRE_FALSE(result.HasValue());
    REQUIRE(result.HasError());
    REQUIRE_FALSE(static_cast<bool>(result));
    REQUIRE(result.GetError().code == Janus::ErrorCode::PlatformInitFailed);
    REQUIRE(result.GetError().message == "platform failed");
}
