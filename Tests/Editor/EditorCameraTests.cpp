#include "EditorCamera.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "EditorCamera maps viewport center to camera position",
    "[editor][camera][v0.6]")
{
    Janus::Editor::EditorCamera camera;

    const auto center =
        camera.ScreenToWorld(
            Janus::Vector2{400.0f, 300.0f},
            Janus::Viewport{800, 600});

    REQUIRE(center.x == Catch::Approx(0.0f));
    REQUIRE(center.y == Catch::Approx(0.0f));

    camera.PanPixels(Janus::Vector2{-10.0f, 5.0f});

    const auto position = camera.GetPosition();
    REQUIRE(position.x == Catch::Approx(10.0f));
    REQUIRE(position.y == Catch::Approx(5.0f));

    const auto movedCenter =
        camera.ScreenToWorld(
            Janus::Vector2{400.0f, 300.0f},
            Janus::Viewport{800, 600});

    REQUIRE(movedCenter.x == Catch::Approx(position.x));
    REQUIRE(movedCenter.y == Catch::Approx(position.y));
}

TEST_CASE(
    "EditorCamera screen to world respects top-left viewport coordinates",
    "[editor][camera][v0.6]")
{
    Janus::Editor::EditorCamera camera;

    const auto topLeft =
        camera.ScreenToWorld(
            Janus::Vector2{0.0f, 0.0f},
            Janus::Viewport{200, 100});

    REQUIRE(topLeft.x == Catch::Approx(-100.0f));
    REQUIRE(topLeft.y == Catch::Approx(50.0f));
}

TEST_CASE(
    "EditorCamera wheel zoom changes world scale and remains bounded",
    "[editor][camera][v0.6]")
{
    Janus::Editor::EditorCamera camera;

    camera.Zoom(1.0f);
    REQUIRE(camera.GetZoom() < 1.0f);

    const auto zoomedPoint =
        camera.ScreenToWorld(
            Janus::Vector2{200.0f, 50.0f},
            Janus::Viewport{200, 100});

    REQUIRE(zoomedPoint.x < 100.0f);

    camera.Zoom(1000.0f);
    REQUIRE(camera.GetZoom() == Catch::Approx(0.05f));

    camera.Zoom(-1000.0f);
    REQUIRE(camera.GetZoom() == Catch::Approx(20.0f));
}

TEST_CASE(
    "EditorCamera render camera mirrors editor state",
    "[editor][camera][v0.6]")
{
    Janus::Editor::EditorCamera camera;
    camera.PanPixels(Janus::Vector2{-20.0f, -30.0f});
    camera.Zoom(2.0f);

    const auto renderCamera = camera.ToRenderCamera();
    const auto position = camera.GetPosition();

    REQUIRE(renderCamera.position.x == Catch::Approx(position.x));
    REQUIRE(renderCamera.position.y == Catch::Approx(position.y));
    REQUIRE(renderCamera.zoom == Catch::Approx(camera.GetZoom()));
    REQUIRE(renderCamera.rotationRadians == Catch::Approx(0.0f));
}
