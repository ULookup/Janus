#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/ScenePose.h"
#include "Scene/SceneRenderer.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <filesystem>
#include <limits>

TEST_CASE("ScenePose resolves the full local parent chain without touching caches",
          "[scene][pose][gizmo]")
{
    using namespace Janus;
    Scene scene;
    const auto root = scene.CreateEntity("Root");
    const auto child = scene.CreateEntity("Child");
    const auto leaf = scene.CreateEntity("Leaf");
    REQUIRE(scene.SetParent(child, root));
    REQUIRE(scene.SetParent(leaf, child));
    auto& rootTransform = *scene.GetComponent<TransformComponent>(root);
    auto& childTransform = *scene.GetComponent<TransformComponent>(child);
    auto& leafTransform = *scene.GetComponent<TransformComponent>(leaf);
    rootTransform.position = {10, 20};
    rootTransform.rotationRadians = 0.7f;
    rootTransform.scale = {2, 3};
    childTransform.position = {4, 5};
    childTransform.rotationRadians = -0.3f;
    childTransform.scale = {4, 0.5f};
    leafTransform.position = {6, 7};
    rootTransform.worldPosition = childTransform.worldPosition =
        leafTransform.worldPosition = {99, 98};
    childTransform.worldRotationRadians = 7;
    childTransform.worldScale = {9, 11};
    childTransform.dirty = false;
    const auto childId = scene.GetComponent<EntityIdentityComponent>(child)->id;
    const auto leafId = scene.GetComponent<EntityIdentityComponent>(leaf)->id;
    const auto rootMatrix = Mat4::Multiply(Mat4::Translate({10, 20}),
                                           Mat4::Multiply(Mat4::Rotate(0.7f), Mat4::Scale({2, 3})));
    const auto childMatrix = Mat4::Multiply(
        rootMatrix, Mat4::Multiply(Mat4::Translate({4, 5}),
                                   Mat4::Multiply(Mat4::Rotate(-0.3f), Mat4::Scale({4, 0.5f}))));
    const auto expected = Mat4::TransformPoint(childMatrix, {6, 7});
    const auto pose = ScenePose::Resolve(scene, leafId);
    REQUIRE(pose);
    CHECK(pose.Value().position.x == Catch::Approx(expected.x));
    CHECK(pose.Value().position.y == Catch::Approx(expected.y));
    CHECK(pose.Value().rotationRadians == Catch::Approx(0.4f));
    CHECK(pose.Value().scale.x == Catch::Approx(8));
    CHECK(pose.Value().scale.y == Catch::Approx(1.5f));
    const auto preview =
        ScenePose::Resolve(scene, leafId, ScenePositionOverride{childId, {14, 25}});
    REQUIRE(preview);
    const auto delta = Mat4::TransformPoint(rootMatrix, {10, 20});
    const auto origin = Mat4::TransformPoint(rootMatrix, {});
    CHECK(preview.Value().position.x == Catch::Approx(expected.x + delta.x - origin.x));
    CHECK(preview.Value().position.y == Catch::Approx(expected.y + delta.y - origin.y));
    const auto matrixOrigin = Mat4::TransformPoint(preview.Value().matrix, {});
    CHECK(matrixOrigin.x == Catch::Approx(preview.Value().position.x));
    CHECK(matrixOrigin.y == Catch::Approx(preview.Value().position.y));
    const auto rootId = scene.GetComponent<EntityIdentityComponent>(root)->id;
    const auto unchangedRoot =
        ScenePose::Resolve(scene, rootId, ScenePositionOverride{childId, {14, 25}});
    REQUIRE(unchangedRoot);
    CHECK(unchangedRoot.Value().position.x == 10);
    CHECK(unchangedRoot.Value().position.y == 20);
    for (const auto entity : {root, child, leaf})
    {
        const auto& transform = *scene.GetComponent<TransformComponent>(entity);
        CHECK(transform.worldPosition.x == 99);
        CHECK(transform.worldPosition.y == 98);
        CHECK(transform.dirty == (entity != child));
    }
    CHECK(childTransform.position.x == 4);
    CHECK(childTransform.position.y == 5);
    CHECK(childTransform.worldRotationRadians == 7);
    CHECK(childTransform.worldScale.x == 9);
    CHECK(childTransform.worldScale.y == 11);
}

TEST_CASE("ScenePose rejects invalid targets, broken chains and nonfinite poses",
          "[scene][pose][gizmo]")
{
    using namespace Janus;
    Scene scene;
    const auto root = scene.CreateEntity("Root");
    const auto child = scene.CreateEntity("Child");
    REQUIRE(scene.SetParent(child, root));
    const auto id = scene.GetComponent<EntityIdentityComponent>(child)->id;
    SECTION("missing entity")
    {
        CHECK_FALSE(ScenePose::Resolve(scene, UUID::Random()));
    }
    SECTION("missing preview entity")
    {
        CHECK_FALSE(ScenePose::Resolve(scene, id, ScenePositionOverride{UUID::Random(), {}}));
    }
    SECTION("invalid preview position")
    {
        CHECK_FALSE(ScenePose::Resolve(
            scene, id, ScenePositionOverride{id, {std::numeric_limits<f32>::infinity(), 0}}));
    }
    SECTION("nonfinite ancestor")
    {
        scene.GetComponent<TransformComponent>(root)->scale.x =
            std::numeric_limits<f32>::quiet_NaN();
        CHECK_FALSE(ScenePose::Resolve(scene, id));
    }
    SECTION("overflow")
    {
        scene.GetComponent<TransformComponent>(root)->scale.x = std::numeric_limits<f32>::max();
        scene.GetComponent<TransformComponent>(child)->scale.x = 2;
        CHECK_FALSE(ScenePose::Resolve(scene, id));
    }
    SECTION("missing parent component")
    {
        REQUIRE(scene.RemoveComponent<TransformComponent>(root));
        CHECK_FALSE(ScenePose::Resolve(scene, id));
    }
    SECTION("cycle")
    {
        scene.GetComponent<HierarchyComponent>(root)->parent = child;
        const auto result = ScenePose::Resolve(scene, id);
        REQUIRE_FALSE(result);
        CHECK(result.GetError().code == ErrorCode::HierarchyCycle);
        scene.GetComponent<HierarchyComponent>(root)->parent = {};
    }
}

TEST_CASE("SceneRenderer preview moves descendants without mutating scene transforms",
          "[scene][render][gizmo]")
{
    using namespace Janus;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetRegistry registry;
    const auto texture = registry.Register(AssetType::Texture, "test_rgba.png");
    REQUIRE(texture);
    AssetService assets(std::filesystem::path(JANUS_TEST_SOURCE_DIR) / "Fixtures" / "Assets",
                        registry, *renderer);
    Scene scene;
    const auto parent = scene.CreateEntity("Parent");
    const auto child = scene.CreateEntity("Child");
    REQUIRE(scene.SetParent(child, parent));
    scene.GetComponent<TransformComponent>(parent)->position = {10, 20};
    scene.GetComponent<TransformComponent>(child)->position = {4, 6};
    scene.AddComponent<SpriteRendererComponent>(child,
                                                SpriteRendererComponent{texture.Value(), {2, 2}});
    SceneRenderer sceneRenderer;
    SceneRenderRequest request{scene, assets, *renderer, {}, {640, 360}, {}};
    request.positionOverride =
        ScenePositionOverride{scene.GetComponent<EntityIdentityComponent>(parent)->id, {30, 50}};
    REQUIRE(sceneRenderer.Render(request));
    REQUIRE(device.vertexUploads.size() == 1);
    f32 vertices[32]{};
    REQUIRE(device.vertexUploads[0].size() == sizeof(vertices));
    std::memcpy(vertices, device.vertexUploads[0].data(), sizeof(vertices));
    CHECK(vertices[0] == Catch::Approx(33));
    CHECK(vertices[1] == Catch::Approx(55));
    for (const auto entity : {parent, child})
    {
        const auto& transform = *scene.GetComponent<TransformComponent>(entity);
        CHECK(transform.worldPosition.x == 0);
        CHECK(transform.worldPosition.y == 0);
        CHECK(transform.dirty);
    }
    CHECK(scene.GetComponent<TransformComponent>(parent)->position.x == 10);
    CHECK(scene.GetComponent<TransformComponent>(parent)->position.y == 20);
    device.vertexUploads.clear();
    request.positionOverride.reset();
    REQUIRE(sceneRenderer.Render(request));
    REQUIRE(device.vertexUploads.size() == 1);
    std::memcpy(vertices, device.vertexUploads[0].data(), sizeof(vertices));
    CHECK(vertices[0] == Catch::Approx(13));
    CHECK(vertices[1] == Catch::Approx(25));
    CHECK_FALSE(scene.GetComponent<TransformComponent>(child)->dirty);
}

TEST_CASE("SceneRenderer rejects invalid previews before drawing or updating caches",
          "[scene][render][gizmo]")
{
    using namespace Janus;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetRegistry registry;
    AssetService assets(".", registry, *renderer);
    Scene scene;
    const auto entity = scene.CreateEntity("Entity");
    SceneRenderer sceneRenderer;
    SceneRenderRequest request{scene, assets, *renderer, {}, {640, 360}, {}};
    request.positionOverride = ScenePositionOverride{UUID::Random(), {1, 2}};
    CHECK_FALSE(sceneRenderer.Render(request));
    CHECK(device.defaultFramebufferBindCount == 0);
    CHECK(scene.GetComponent<TransformComponent>(entity)->dirty);
}

TEST_CASE("Scene render requests preserve the host background color", "[scene][render][editor]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetRegistry registry;
    Janus::AssetService assets(".", registry, *renderer);
    Janus::Scene scene;
    Janus::SceneRenderer sceneRenderer;
    Janus::SceneRenderRequest request{scene, assets, *renderer, {}, {640, 360}, {}};
    request.clearColor = {0.1f, 0.2f, 0.3f, 1};
    REQUIRE(sceneRenderer.Render(request));
    REQUIRE(device.lastClearColor.r == Catch::Approx(0.1f));
    REQUIRE(device.lastClearColor.g == Catch::Approx(0.2f));
}

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
