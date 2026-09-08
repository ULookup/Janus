#include "EditorWorkspaceLayout.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

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

TEST_CASE("Editor workspace presets distinguish standard focus and debug without overlaps",
          "[editor][workspace][preferences]")
{
    using namespace Janus::Editor;
    const auto standard = BuildEditorWorkspaceLayout(
        1440, 900, GetDefaultWorkspacePreferences(EditorLayoutMode::Standard));
    const auto focus = BuildEditorWorkspaceLayout(
        1440, 900, GetDefaultWorkspacePreferences(EditorLayoutMode::Focus));
    const auto debug = BuildEditorWorkspaceLayout(
        1440, 900, GetDefaultWorkspacePreferences(EditorLayoutMode::Debug));
    CHECK(focus.viewport.width > standard.viewport.width);
    CHECK(focus.viewport.height > standard.viewport.height);
    CHECK(focus.hierarchy.width == 0);
    CHECK(focus.inspector.width == 0);
    CHECK(focus.assets.width == 0);
    CHECK(focus.diagnostics.width == 0);
    CHECK(debug.diagnostics.width > debug.assets.width);
    CHECK(debug.utility.height > standard.utility.height);
    for (const auto mode :
         {EditorLayoutMode::Standard, EditorLayoutMode::Focus, EditorLayoutMode::Debug})
        for (const auto width : {0.0f, 320.0f, 720.0f, 1440.0f})
        {
            const auto layout =
                BuildEditorWorkspaceLayout(width, 480, GetDefaultWorkspacePreferences(mode));
            for (const auto panel :
                 {layout.toolbar, layout.hierarchy, layout.viewport, layout.inspector,
                  layout.assets, layout.diagnostics, layout.status})
            {
                CHECK(panel.x >= 0);
                CHECK(panel.y >= 0);
                CHECK(panel.x + panel.width <= width + 0.01f);
                CHECK(panel.y + panel.height <= 480.01f);
            }
        }
}

TEST_CASE("Editor workspace rejects nonfinite dimensions through bounded fallback",
          "[editor][workspace][preferences]")
{
    Janus::Editor::EditorWorkspacePreferences preferences;
    preferences.leftWidth = std::numeric_limits<float>::quiet_NaN();
    preferences.rightWidth = std::numeric_limits<float>::infinity();
    preferences.utilityHeight = -std::numeric_limits<float>::infinity();
    const auto layout = Janus::Editor::BuildEditorWorkspaceLayout(1440, 900, preferences);
    for (const auto panel : {layout.hierarchy, layout.viewport, layout.inspector, layout.utility})
    {
        CHECK(std::isfinite(panel.x));
        CHECK(std::isfinite(panel.y));
        CHECK(std::isfinite(panel.width));
        CHECK(std::isfinite(panel.height));
    }
}

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
    REQUIRE(layout.toolbar.height <= 100.0f);

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

    REQUIRE(layout.utility.width == Catch::Approx(layout.inspector.x - 6.0f));
    REQUIRE(layout.inspector.height > layout.viewport.height);
    REQUIRE(layout.diagnostics.width > 0.0f);
    REQUIRE_FALSE(Overlaps(layout.assets, layout.diagnostics));
    REQUIRE_FALSE(Overlaps(layout.status, layout.inspector));
}

TEST_CASE("Editor workspace clamps user splits and collapses diagnostics on narrow windows",
          "[editor][workspace]")
{
    Janus::Editor::EditorWorkspacePreferences preferences;
    preferences.leftWidth = 900.0f;
    preferences.rightWidth = 900.0f;
    preferences.utilityHeight = 900.0f;
    for (const auto width : {0.0f, 320.0f, 720.0f, 1440.0f})
    {
        const auto layout = Janus::Editor::BuildEditorWorkspaceLayout(width, 480, preferences);
        for (const auto& panel :
             {layout.toolbar, layout.hierarchy, layout.viewport, layout.inspector, layout.utility,
              layout.assets, layout.diagnostics, layout.status})
        {
            REQUIRE(panel.x >= 0);
            REQUIRE(panel.y >= 0);
            REQUIRE(panel.width >= 0);
            REQUIRE(panel.height >= 0);
            REQUIRE(panel.x + panel.width <= width + 0.01f);
            REQUIRE(panel.y + panel.height <= 480.01f);
        }
        if (width < 1200)
            REQUIRE(layout.diagnostics.width == 0);
    }
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

TEST_CASE("Asset preview fitting preserves portrait and atlas proportions",
          "[editor][workspace][preview]")
{
    for (const float ratio : {0.25f, 1.0f, 4.0f})
    {
        const auto fit = Janus::Editor::FitAspectRatio(96, 96, ratio);
        REQUIRE(fit.width / fit.height == Catch::Approx(ratio));
        REQUIRE(fit.x + fit.width <= 96);
        REQUIRE(fit.y + fit.height <= 96);
        REQUIRE(fit.x * 2 + fit.width == Catch::Approx(96));
        REQUIRE(fit.y * 2 + fit.height == Catch::Approx(96));
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
