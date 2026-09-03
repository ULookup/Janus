#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderer.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("SceneRenderer resolves persistent sprite assets through AssetService",
          "[scene][render][asset]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::AssetRegistry registry;
    const auto handleResult =
        registry.Register(Janus::AssetType::Texture, "test_rgba.png");
    REQUIRE(handleResult);

    const auto projectRoot =
        std::filesystem::path(JANUS_TEST_SOURCE_DIR)
        / "Fixtures"
        / "Assets";
    Janus::AssetService assets(projectRoot, registry, *renderer);
    Janus::SceneRenderer sceneRenderer;

    Janus::Scene scene;
    const auto camera = scene.CreateEntity("Camera");
    scene.AddComponent<Janus::CameraComponent>(
        camera,
        Janus::CameraComponent{1.0f, true});

    const auto spriteEntity = scene.CreateEntity("Sprite");
    scene.AddComponent<Janus::SpriteRendererComponent>(
        spriteEntity,
        Janus::SpriteRendererComponent{
            handleResult.Value(),
            Janus::Vector2{2.0f, 3.0f}});

    REQUIRE(sceneRenderer.Render(
        scene,
        assets,
        *renderer,
        Janus::Viewport{800, 600}));

    REQUIRE(device.createdTextures.size() == 1);
    REQUIRE(device.drawCommands.size() == 1);
    REQUIRE(device.drawCommands[0].texture.value
            == device.createdTextures[0].handle.value);

    device.drawCommands.clear();
    REQUIRE(sceneRenderer.Render(
        scene,
        assets,
        *renderer,
        Janus::Viewport{800, 600}));

    REQUIRE(device.createdTextures.size() == 1);
    REQUIRE(device.drawCommands.size() == 1);
}

TEST_CASE("SceneRenderer reports unresolved persistent sprite assets",
          "[scene][render][asset]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetRegistry registry;
    Janus::AssetService assets(".", registry, *renderer);
    Janus::SceneRenderer sceneRenderer;

    Janus::Scene scene;
    const auto camera = scene.CreateEntity("Camera");
    scene.AddComponent<Janus::CameraComponent>(
        camera,
        Janus::CameraComponent{1.0f, true});

    const auto spriteEntity = scene.CreateEntity("Missing Sprite");
    scene.AddComponent<Janus::SpriteRendererComponent>(
        spriteEntity,
        Janus::SpriteRendererComponent{
            Janus::AssetHandle::Random(),
            Janus::Vector2{2.0f, 3.0f}});

    const auto result = sceneRenderer.Render(
        scene,
        assets,
        *renderer,
        Janus::Viewport{800, 600});

    REQUIRE_FALSE(result);
    REQUIRE(result.GetError().code == Janus::ErrorCode::AssetNotFound);
    REQUIRE(device.createdTextures.empty());
}
