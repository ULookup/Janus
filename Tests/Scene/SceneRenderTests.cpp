#include "Scene/Scene.h"

#include "../Renderer/FakeRenderDevice.h"
#include "Renderer/Renderer2D.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Scene renders camera and sprite through Renderer2D", "[scene][render]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::Scene scene;

    const auto cameraEntity = scene.CreateEntity();
    scene.AddComponent<Janus::CameraComponent>(
        cameraEntity,
        Janus::CameraComponent{1.0f, true});

    const auto spriteEntity = scene.CreateEntity();
    auto* transform =
        scene.GetComponent<Janus::TransformComponent>(
            spriteEntity);
    transform->position = Janus::Vector2{10.0f, 20.0f};

    scene.AddComponent<Janus::SpriteRendererComponent>(
        spriteEntity,
        Janus::SpriteRendererComponent{
            Janus::TextureHandle{1},
            Janus::Vector2{2.0f, 3.0f}});

    const auto result =
        scene.Render(*renderer, Janus::Viewport{800, 600});

    REQUIRE(result);
    REQUIRE(device.drawCommands.size() == 1);
    REQUIRE(renderer->GetStatistics().spriteCount == 1);
    REQUIRE(renderer->GetStatistics().drawCallCount == 1);
}

TEST_CASE("Scene reports missing camera", "[scene][render]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::Scene scene;

    const auto result =
        scene.Render(*renderer, Janus::Viewport{800, 600});

    REQUIRE_FALSE(result);
    REQUIRE(result.GetError().code == Janus::ErrorCode::CameraNotFound);
}
