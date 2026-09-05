#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderer.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE(
    "SceneRenderer resolves persistent sprite assets through AssetService",
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
    REQUIRE(
        device.drawCommands[0].texture.value
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

TEST_CASE(
    "SceneRenderer reports unresolved persistent sprite assets",
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
    REQUIRE(
        result.GetError().code
        == Janus::ErrorCode::AssetNotFound);
    REQUIRE(device.createdTextures.empty());
    REQUIRE(device.defaultFramebufferBindCount == 0);
}

TEST_CASE(
    "SceneRenderer renders with an explicit camera without Scene camera",
    "[scene][render][camera][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::AssetRegistry registry;
    const auto texture =
        registry.Register(Janus::AssetType::Texture, "test_rgba.png");
    REQUIRE(texture);

    const auto projectRoot =
        std::filesystem::path(JANUS_TEST_SOURCE_DIR)
        / "Fixtures"
        / "Assets";
    Janus::AssetService assets(projectRoot, registry, *renderer);
    Janus::SceneRenderer sceneRenderer;

    Janus::Scene scene;
    const auto spriteEntity = scene.CreateEntity("Sprite");
    scene.AddComponent<Janus::SpriteRendererComponent>(
        spriteEntity,
        Janus::SpriteRendererComponent{
            texture.Value(),
            Janus::Vector2{32.0f, 32.0f}});

    const auto target =
        renderer->CreateRenderTarget(
            Janus::RenderTargetDesc{320, 180});
    REQUIRE(target);

    Janus::OrthographicCamera editorCamera;
    editorCamera.position = {50.0f, -20.0f};
    editorCamera.zoom = 2.0f;

    REQUIRE(sceneRenderer.Render(
        Janus::SceneRenderRequest{
            scene,
            assets,
            *renderer,
            editorCamera,
            Janus::Viewport{320, 180},
            target.Value()}));

    REQUIRE(device.drawCommands.size() == 1);
    REQUIRE(device.boundFramebuffers.size() == 1);
    REQUIRE(device.defaultFramebufferBindCount == 1);
}

TEST_CASE(
    "SceneRenderer resolves primary Scene camera from world transform",
    "[scene][render][camera][v0.6]")
{
    Janus::SceneRenderer sceneRenderer;
    Janus::Scene scene;

    const auto cameraEntity = scene.CreateEntity("Camera");
    auto* transform =
        scene.GetComponent<Janus::TransformComponent>(cameraEntity);
    REQUIRE(transform != nullptr);

    transform->position = {12.0f, -8.0f};
    transform->rotationRadians = 0.25f;

    scene.AddComponent<Janus::CameraComponent>(
        cameraEntity,
        Janus::CameraComponent{2.5f, true});

    const auto camera =
        sceneRenderer.ResolvePrimaryCamera(scene);

    REQUIRE(camera);
    REQUIRE(camera.Value().position.x == Catch::Approx(12.0f));
    REQUIRE(camera.Value().position.y == Catch::Approx(-8.0f));
    REQUIRE(
        camera.Value().rotationRadians
        == Catch::Approx(0.25f));
    REQUIRE(camera.Value().zoom == Catch::Approx(2.5f));
}
