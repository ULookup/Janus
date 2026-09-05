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
    "Editor workspace gives Scene View the largest central region",
    "[editor][workspace][v0.6]")
{
    const auto layout =
        Janus::Editor::BuildEditorWorkspaceLayout(
            1440.0f,
            900.0f);

    REQUIRE(layout.toolbar.width == Catch::Approx(1440.0f));
    REQUIRE(layout.toolbar.y == Catch::Approx(0.0f));
    REQUIRE(layout.toolbar.height >= 40.0f);
    REQUIRE(layout.toolbar.height <= 52.0f);

    REQUIRE(layout.sceneView.width > layout.hierarchy.width);
    REQUIRE(layout.sceneView.width > layout.inspector.width);
    REQUIRE(layout.sceneView.height > layout.gameView.height);

    REQUIRE_FALSE(
        Overlaps(
            layout.hierarchy,
            layout.sceneView));
    REQUIRE_FALSE(
        Overlaps(
            layout.sceneView,
            layout.inspector));
    REQUIRE_FALSE(
        Overlaps(
            layout.hierarchy,
            layout.assetBrowser));
    REQUIRE_FALSE(
        Overlaps(
            layout.sceneView,
            layout.gameView));
    REQUIRE_FALSE(
        Overlaps(
            layout.inspector,
            layout.console));
}

TEST_CASE(
    "Editor workspace stays non-negative at compact window sizes",
    "[editor][workspace][v0.6]")
{
    const auto layout =
        Janus::Editor::BuildEditorWorkspaceLayout(
            720.0f,
            480.0f);

    const Janus::Editor::EditorPanelRect panels[] = {
        layout.toolbar,
        layout.hierarchy,
        layout.assetBrowser,
        layout.sceneView,
        layout.gameView,
        layout.inspector,
        layout.console};

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
