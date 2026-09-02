#include "Scene/Scene.h"

#include "../Renderer/FakeRenderDevice.h"
#include "Renderer/Renderer2D.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Scene propagates transform hierarchy", "[scene][transform]")
{
    Janus::Scene scene;
    const auto parent = scene.CreateEntity();
    const auto child = scene.CreateEntity();

    REQUIRE(scene.SetParent(child, parent));

    const auto camera = scene.CreateEntity();
    scene.AddComponent<Janus::CameraComponent>(
        camera,
        Janus::CameraComponent{1.0f, true});

    auto* parentTransform =
        scene.GetComponent<Janus::TransformComponent>(parent);
    auto* childTransform =
        scene.GetComponent<Janus::TransformComponent>(child);

    parentTransform->position = Janus::Vector2{10.0f, 20.0f};
    childTransform->position = Janus::Vector2{5.0f, 6.0f};
    childTransform->scale = Janus::Vector2{2.0f, 3.0f};

    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);

    REQUIRE(scene.Render(*renderer, Janus::Viewport{800, 600}));

    REQUIRE(childTransform->worldPosition.x == Catch::Approx(15.0f));
    REQUIRE(childTransform->worldPosition.y == Catch::Approx(26.0f));
    REQUIRE(childTransform->worldScale.x == Catch::Approx(2.0f));
    REQUIRE(childTransform->worldScale.y == Catch::Approx(3.0f));
}
