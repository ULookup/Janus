#include "../Renderer/FakeRenderDevice.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneRenderer.h"
#include "UI/UIComponents.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <filesystem>

using namespace Janus;

TEST_CASE("Game world and UI retain the same composition when the render target shrinks",
          "[acceptance][ui][render]")
{
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    const auto root = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "Game";
    auto scene = SceneDeserializer::Load(root / "Scenes/Integrated.scene", reflection.Value());
    REQUIRE(scene);
    auto registry = AssetRegistry::Load(root / "Config/AssetRegistry.json");
    REQUIRE(registry);
    AssetService assets(root, registry.Value(), *renderer);
    SceneRenderer sceneRenderer;
    for (const auto viewport : {Viewport{1280, 720}, Viewport{640, 360}, Viewport{800, 600}})
    {
        device.drawProjections.clear();
        auto target = renderer->CreateRenderTarget({viewport.width, viewport.height});
        REQUIRE(target);
        auto camera = sceneRenderer.ResolvePrimaryCamera(*scene.Value());
        REQUIRE(camera);
        REQUIRE(sceneRenderer.Render(SceneRenderRequest{*scene.Value(),
                                                        assets,
                                                        *renderer,
                                                        camera.Value(),
                                                        viewport,
                                                        target.Value(),
                                                        {1280, 720}}));
        REQUIRE(device.drawProjections.size() >= 2);
        auto worldPoint = Mat4::TransformPoint(device.drawProjections.front(), {3, 1.5f});
        CHECK(worldPoint.x == Catch::Approx(0.3f));
        CHECK(worldPoint.y == Catch::Approx(192.0f / 720));
        auto uiPoint = Mat4::TransformPoint(device.drawProjections.back(), {832, 264});
        CHECK(uiPoint.x == Catch::Approx(worldPoint.x));
        CHECK(uiPoint.y == Catch::Approx(worldPoint.y));
        REQUIRE(renderer->DestroyRenderTarget(target.Value()));
    }
}

TEST_CASE("UI showcase loads through production persistence and renders its registered assets",
          "[ui][render]")
{
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    const auto project =
        std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "SandboxProject";
    auto scene =
        SceneDeserializer::Load(project / "Scenes" / "UIShowcase.scene", reflection.Value());
    REQUIRE(scene);
    auto registry = AssetRegistry::Load(project / "Config" / "AssetRegistry.json");
    REQUIRE(registry);
    AssetService assets(project, registry.Value(), *renderer);
    SceneRenderer sceneRenderer;
    REQUIRE(sceneRenderer.Render(*scene.Value(), assets, *renderer, {800, 450}, {1280, 720}));
    CHECK(renderer->GetStatistics().spriteCount == 13);
}

TEST_CASE("UI Image crops geometry and UV together at logical resolution", "[ui][render]")
{
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetRegistry registry;
    auto texture = registry.Register(AssetType::Texture, "test_rgba.png");
    REQUIRE(texture);
    AssetService assets(std::filesystem::path(JANUS_TEST_SOURCE_DIR) / "Fixtures" / "Assets",
                        registry, *renderer);
    Scene scene;
    auto canvas = scene.CreateEntity();
    scene.AddComponent<CanvasComponent>(canvas, {});
    auto entity = scene.CreateEntity();
    REQUIRE(scene.SetParent(entity, canvas));
    UIRectComponent rect;
    rect.offset = {-25, -20};
    rect.size = {100, 80};
    scene.AddComponent<UIRectComponent>(entity, rect);
    ImageComponent image;
    image.texture.id = texture.Value().id;
    scene.AddComponent<ImageComponent>(entity, image);
    SceneRenderer sceneRenderer;
    REQUIRE(sceneRenderer.Render(scene, assets, *renderer, {400, 300}, {800, 600}));
    REQUIRE(device.vertexUploads.size() == 1);
    // Sprite vertex ABI: position float2, UV float2, color float4.
    REQUIRE(device.vertexUploads[0].size() == sizeof(f32) * 32);
    f32 vertices[32];
    std::memcpy(vertices, device.vertexUploads[0].data(), sizeof(vertices));
    CHECK(vertices[0] == 0);
    CHECK(vertices[1] == 0);
    CHECK(vertices[2] == Catch::Approx(0.25f));
    CHECK(vertices[3] == Catch::Approx(0.25f));
    CHECK(vertices[8] == 75);
    CHECK(vertices[17] == 60);
    REQUIRE(device.viewProjections.size() >= 2);
    REQUIRE(device.drawProjections.size() == 1);
    auto topLeft = Mat4::TransformPoint(device.drawProjections.back(), {0, 0});
    auto bottomRight = Mat4::TransformPoint(device.drawProjections.back(), {800, 600});
    CHECK(topLeft.x == Catch::Approx(-1));
    CHECK(topLeft.y == Catch::Approx(1));
    CHECK(bottomRight.x == Catch::Approx(1));
    CHECK(bottomRight.y == Catch::Approx(-1));
}
TEST_CASE(
    "UI overlay draws after world and preserves target statistics and white texture ownership",
    "[ui][render]")
{
    Test::FakeRenderDevice device;
    {
        auto renderer = Detail::Renderer2DTestAccess::Create(device);
        RenderFrameDesc frame;
        frame.viewport = {800, 600};
        auto target = renderer->CreateRenderTarget({800, 600});
        REQUIRE(target);
        frame.target = target.Value();
        REQUIRE(renderer->BeginFrame(frame));
        Sprite world;
        world.texture = {100};
        renderer->SubmitSprite(world);
        std::vector<Sprite> overlay(3);
        overlay[0].texture = {200};
        overlay[1].texture = {};
        overlay[2].texture = {200};
        REQUIRE(renderer->EndFrame(overlay, {1280, 720}));
        REQUIRE(device.drawCommands.size() == 4);
        CHECK(device.drawCommands[0].texture.value == 100);
        CHECK(device.drawCommands[1].texture.value == 200);
        CHECK(device.drawCommands[2].texture.value != 0);
        CHECK(device.drawCommands[3].texture.value == 200);
        CHECK(renderer->GetStatistics().spriteCount == 4);
        CHECK(device.defaultFramebufferBindCount == 1);
        REQUIRE(device.createdTextures.size() == 2);
        CHECK(device.createdTextures.back().pixels == std::vector<u8>{255, 255, 255, 255});
    }
    CHECK(device.destroyedTextures.size() == 2);
}

TEST_CASE("UI overlay failure releases offscreen target for subsequent frames", "[ui][render]")
{
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    RenderFrameDesc frame;
    frame.viewport = {800, 600};
    frame.target = renderer->CreateRenderTarget({800, 600}).Value();
    std::vector<Sprite> overlay(1);
    REQUIRE(renderer->BeginFrame(frame));
    device.failNextTextureCreate = true;
    CHECK_FALSE(renderer->EndFrame(overlay, {800, 600}));
    REQUIRE(renderer->BeginFrame(frame));
    CHECK_FALSE(renderer->EndFrame(overlay, {0, 600}));
    REQUIRE(renderer->BeginFrame(frame));
    REQUIRE(renderer->EndFrame(overlay, {800, 600}));
}

TEST_CASE("UI-only scene renders without a camera and rejects unsupported canvas topology",
          "[ui][render]")
{
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetRegistry registry;
    AssetService assets(".", registry, *renderer);
    Scene scene;
    SceneRenderer sceneRenderer;
    const auto canvas = scene.CreateEntity("Canvas");
    scene.AddComponent<CanvasComponent>(canvas, {});
    scene.AddComponent<PanelComponent>(canvas, {});
    REQUIRE(sceneRenderer.Render(scene, assets, *renderer, {800, 600}, {1280, 720}));
    REQUIRE(device.drawCommands.size() == 1);
    const auto other = scene.CreateEntity();
    scene.AddComponent<CanvasComponent>(other, {});
    CHECK_FALSE(sceneRenderer.Render(scene, assets, *renderer, {800, 600}));
    CHECK(device.drawCommands.size() == 1);
}
