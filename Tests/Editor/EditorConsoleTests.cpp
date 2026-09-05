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
