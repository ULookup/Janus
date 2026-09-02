#include "Core/Math/Mat4.h"
#include "Core/Math/Vector2.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/RendererTypes.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Color defaults to opaque white", "[renderer][types]")
{
    const auto color = Janus::Color::White();
    REQUIRE(color.r == Catch::Approx(1.0f));
    REQUIRE(color.g == Catch::Approx(1.0f));
    REQUIRE(color.b == Catch::Approx(1.0f));
    REQUIRE(color.a == Catch::Approx(1.0f));
}

TEST_CASE("TextureRegion full covers normalized UV space", "[renderer][types]")
{
    const auto region = Janus::TextureRegion::Full();
    REQUIRE(region.min.x == Catch::Approx(0.0f));
    REQUIRE(region.min.y == Catch::Approx(0.0f));
    REQUIRE(region.max.x == Catch::Approx(1.0f));
    REQUIRE(region.max.y == Catch::Approx(1.0f));
}

TEST_CASE("OrthographicCamera zoom changes view projection scale", "[renderer][camera]")
{
    Janus::OrthographicCamera camera;
    camera.position = Janus::Vector2{640.0f, 360.0f};

    const auto matrix = camera.ViewProjection(Janus::Viewport{1280, 720});

    const auto transformed = Janus::Mat4::TransformPoint(
        matrix,
        camera.position);

    REQUIRE(transformed.x == Catch::Approx(0.0f).margin(0.001f));
    REQUIRE(transformed.y == Catch::Approx(0.0f).margin(0.001f));
}
