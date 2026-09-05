#include "EditorWorkspaceLayout.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{

bool Overlaps(
    const Janus::Editor::EditorPanelRect& left,
    const Janus::Editor::EditorPanelRect& right)
{
    return left.x < right.x + right.width
        && left.x + left.width > right.x
        && left.y < right.y + right.height
        && left.y + left.height > right.y;
}

} // namespace

TEST_CASE(
    "Editor workspace prioritizes one central viewport",
    "[editor][workspace][v0.6]")
{
    const auto layout =
        Janus::Editor::BuildEditorWorkspaceLayout(
            1440.0f,
            900.0f);

    REQUIRE(layout.toolbar.width == Catch::Approx(1440.0f));
    REQUIRE(layout.toolbar.y == Catch::Approx(0.0f));
    REQUIRE(layout.toolbar.height >= 42.0f);
    REQUIRE(layout.toolbar.height <= 50.0f);

    REQUIRE(layout.viewport.width > layout.hierarchy.width);
    REQUIRE(layout.viewport.width > layout.inspector.width);
    REQUIRE(layout.viewport.height > layout.utility.height);

    REQUIRE_FALSE(
        Overlaps(
            layout.hierarchy,
            layout.viewport));
    REQUIRE_FALSE(
        Overlaps(
            layout.viewport,
            layout.inspector));
    REQUIRE_FALSE(
        Overlaps(
            layout.hierarchy,
            layout.utility));
    REQUIRE_FALSE(
        Overlaps(
            layout.viewport,
            layout.utility));
    REQUIRE_FALSE(
        Overlaps(
            layout.inspector,
            layout.utility));

    REQUIRE(
        layout.utility.width
        == Catch::Approx(1440.0f));
}

TEST_CASE(
    "Editor workspace stays in bounds at compact window sizes",
    "[editor][workspace][v0.6]")
{
    const auto layout =
        Janus::Editor::BuildEditorWorkspaceLayout(
            720.0f,
            480.0f);

    const Janus::Editor::EditorPanelRect panels[] = {
        layout.toolbar,
        layout.hierarchy,
        layout.viewport,
        layout.inspector,
        layout.utility};

    for (const auto& panel : panels)
    {
        REQUIRE(panel.x >= 0.0f);
        REQUIRE(panel.y >= 0.0f);
        REQUIRE(panel.width >= 0.0f);
        REQUIRE(panel.height >= 0.0f);
        REQUIRE(panel.x + panel.width <= 720.01f);
        REQUIRE(panel.y + panel.height <= 480.01f);
    }
}

TEST_CASE(
    "Aspect fitting preserves 16 by 9 without cropping",
    "[editor][workspace][game-view][v0.6]")
{
    const auto wide =
        Janus::Editor::FitAspectRatio(
            1000.0f,
            400.0f,
            16.0f / 9.0f);

    REQUIRE(wide.height == Catch::Approx(400.0f));
    REQUIRE(
        wide.width / wide.height
        == Catch::Approx(16.0f / 9.0f));
    REQUIRE(wide.x > 0.0f);
    REQUIRE(wide.y == Catch::Approx(0.0f));

    const auto tall =
        Janus::Editor::FitAspectRatio(
            800.0f,
            800.0f,
            16.0f / 9.0f);

    REQUIRE(tall.width == Catch::Approx(800.0f));
    REQUIRE(
        tall.width / tall.height
        == Catch::Approx(16.0f / 9.0f));
    REQUIRE(tall.x == Catch::Approx(0.0f));
    REQUIRE(tall.y > 0.0f);
}
