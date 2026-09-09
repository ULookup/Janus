#include "EditorConsole.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "EditorConsole preserves bounded error history",
    "[editor][console][v0.6]")
{
    Janus::Editor::EditorConsole console(2);

    console.PushInfo("first");
    console.PushError(
        Janus::Error{
            Janus::ErrorCode::InvalidState,
            "second"});
    console.PushInfo("third");

    const auto& entries = console.GetEntries();

    REQUIRE(console.GetCapacity() == 2);
    REQUIRE(entries.size() == 2);
    REQUIRE(
        entries[0].level
        == Janus::Editor::EditorConsoleLevel::Error);
    REQUIRE(entries[0].message == "second");
    REQUIRE(
        entries[1].level
        == Janus::Editor::EditorConsoleLevel::Info);
    REQUIRE(entries[1].message == "third");

    console.Clear();
    REQUIRE(console.GetEntries().empty());
}

TEST_CASE(
    "EditorConsole zero capacity still retains latest entry",
    "[editor][console][v0.6]")
{
    Janus::Editor::EditorConsole console(0);

    console.PushInfo("message");

    REQUIRE(console.GetCapacity() == 1);
    REQUIRE(console.GetEntries().size() == 1);
}

TEST_CASE("Console level filters query the shared structured store", "[console][v0.9]")
{
    using namespace Janus;
    auto store = std::make_shared<LogStore>();
    store->Append(LogLevel::Info, "Test", "info");
    store->Append(LogLevel::Warning, "Test", "warning");
    Editor::EditorConsole console(store);
    console.SetLevelFilter(Editor::EditorConsoleLevel::Warning);
    REQUIRE(console.GetEntries().size() == 1);
    REQUIRE(console.GetEntries()[0].message == "warning");
    console.SetLevelFilter({});
    REQUIRE(console.GetEntries().size() == 2);
}

TEST_CASE("Console search and counts cover retained logs beyond one protocol page", "[console][d2]")
{
    using namespace Janus;
    auto store = std::make_shared<LogStore>(300);
    const auto runtime = UUID::Random();
    store->Append(LogLevel::Error, "Lua", "first failure", {runtime, 7, ErrorCode::InvalidState});
    for (int i = 0; i < 240; ++i)
        store->Append(LogLevel::Info, "Renderer", "frame");
    store->Append(LogLevel::Warning, "Asset", "missing texture");
    Editor::EditorConsole console(store);
    console.SetSearch("FAILURE");
    auto view = console.Read();
    REQUIRE(view.entries.size() == 1);
    CHECK(view.counts[0] == 240);
    CHECK(view.counts[1] == 1);
    CHECK(view.counts[2] == 1);
    const auto& entry = view.entries.front();
    CHECK(entry.sequence == 1);
    CHECK(entry.timestampMilliseconds > 0);
    CHECK(entry.category == "Lua");
    CHECK(entry.context.runtimeId == runtime);
    CHECK(entry.context.frameIndex == 7);
    CHECK(entry.context.errorCode == ErrorCode::InvalidState);
    console.SetSearch("renderer");
    CHECK(console.Read().entries.size() == 240);
    console.SetRuntimeFilter(runtime);
    CHECK(console.Read().entries.empty());
    console.SetSearch("");
    CHECK(console.Read().entries.size() == 1);
    console.SetLevelFilter(Editor::EditorConsoleLevel::Warning);
    CHECK(console.Read().entries.empty());
    console.Clear();
    CHECK(store->Read().Value().entries.empty());
    CHECK(console.Read().counts == std::array<usize, 3>{});
}

TEST_CASE("Console refresh discards evicted details and preserves stable log identities",
          "[console][d2]")
{
    using namespace Janus;
    auto store = std::make_shared<LogStore>(2);
    Editor::EditorConsole console(store);
    store->Append(LogLevel::Error, "Lua", "old");
    const auto selected = console.Read().entries.front().sequence;
    store->Append(LogLevel::Info, "Editor", "new");
    store->Append(LogLevel::Warning, "Asset", std::string(9000, 'x'));
    const auto view = console.Read();
    REQUIRE(view.entries.size() == 2);
    CHECK(view.entries.front().sequence != selected);
    CHECK(view.entries.back().truncated);
    CHECK(view.droppedCount == 1);
    CHECK(view.counts == std::array<usize, 3>{1, 1, 0});
}
