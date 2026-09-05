#include "Host/McpMainThreadDispatcher.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <future>
#include <thread>

namespace
{

bool WaitForPending(
    Janus::MCP::McpMainThreadDispatcher& dispatcher,
    Janus::usize expected)
{
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() < deadline)
    {
        if (dispatcher.GetPendingCount() == expected)
        {
            return true;
        }

        std::this_thread::yield();
    }

    return false;
}

} // namespace

TEST_CASE(
    "MCP dispatcher executes worker requests only when main thread pumps",
    "[mcp][host][dispatcher][v0.8]")
{
    using namespace Janus::MCP;

    McpMainThreadDispatcher dispatcher;

    const std::thread::id owner =
        std::this_thread::get_id();

    auto future =
        std::async(
            std::launch::async,
            [&dispatcher, owner]()
            {
                return dispatcher.Invoke(
                    [owner]() -> McpDispatchResult
                    {
                        REQUIRE(
                            std::this_thread::get_id()
                            == owner);

                        return Json{
                            {"ok", true}};
                    });
            });

    REQUIRE(
        WaitForPending(
            dispatcher,
            1));

    REQUIRE(
        future.wait_for(
            std::chrono::milliseconds(0))
        == std::future_status::timeout);

    const auto pumped =
        dispatcher.Pump();

    REQUIRE(pumped);
    REQUIRE(pumped.Value() == 1);

    const McpDispatchResult result =
        future.get();

    REQUIRE(
        std::holds_alternative<Json>(
            result));
    REQUIRE(
        std::get<Json>(result).at("ok")
        == true);
}

TEST_CASE(
    "MCP dispatcher enforces bounded per-pump work",
    "[mcp][host][dispatcher][v0.8]")
{
    using namespace Janus::MCP;

    McpMainThreadDispatcher dispatcher(1);

    auto first =
        std::async(
            std::launch::async,
            [&dispatcher]()
            {
                return dispatcher.Invoke(
                    []() -> McpDispatchResult
                    {
                        return Json{{"id", 1}};
                    });
            });

    REQUIRE(
        WaitForPending(
            dispatcher,
            1));

    auto second =
        std::async(
            std::launch::async,
            [&dispatcher]()
            {
                return dispatcher.Invoke(
                    []() -> McpDispatchResult
                    {
                        return Json{{"id", 2}};
                    });
            });

    REQUIRE(
        WaitForPending(
            dispatcher,
            2));

    REQUIRE(dispatcher.Pump().Value() == 1);
    REQUIRE(dispatcher.GetPendingCount() == 1);
    REQUIRE(dispatcher.Pump().Value() == 1);

    REQUIRE(
        std::holds_alternative<Json>(
            first.get()));
    REQUIRE(
        std::holds_alternative<Json>(
            second.get()));
}

TEST_CASE(
    "MCP dispatcher stop releases queued workers",
    "[mcp][host][dispatcher][v0.8]")
{
    using namespace Janus::MCP;

    McpMainThreadDispatcher dispatcher;

    auto future =
        std::async(
            std::launch::async,
            [&dispatcher]()
            {
                return dispatcher.Invoke(
                    []() -> McpDispatchResult
                    {
                        return Json{
                            {"shouldRun", false}};
                    });
            });

    REQUIRE(
        WaitForPending(
            dispatcher,
            1));

    dispatcher.Stop();

    const McpDispatchResult result =
        future.get();

    REQUIRE(
        std::holds_alternative<McpDispatchError>(
            result));
    REQUIRE(dispatcher.IsStopped());
    REQUIRE(dispatcher.GetPendingCount() == 0);
}

TEST_CASE(
    "MCP dispatcher rejects pump from non-owner thread",
    "[mcp][host][dispatcher][v0.8]")
{
    Janus::MCP::McpMainThreadDispatcher dispatcher;

    auto future =
        std::async(
            std::launch::async,
            [&dispatcher]()
            {
                return dispatcher.Pump();
            });

    const auto result =
        future.get();

    REQUIRE_FALSE(result);
    REQUIRE(
        result.GetError().code
        == Janus::ErrorCode::InvalidState);
}
