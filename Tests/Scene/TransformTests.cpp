#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderer.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("SceneRenderer propagates transform hierarchy", "[scene][transform]")
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
    Janus::AssetRegistry registry;
    Janus::AssetService assets(".", registry, *renderer);
    Janus::SceneRenderer sceneRenderer;

    REQUIRE(sceneRenderer.Render(
        scene,
        assets,
        *renderer,
        Janus::Viewport{800, 600}));

    REQUIRE(childTransform->worldPosition.x == Catch::Approx(15.0f));
    REQUIRE(childTransform->worldPosition.y == Catch::Approx(26.0f));
    REQUIRE(childTransform->worldScale.x == Catch::Approx(2.0f));
    REQUIRE(childTransform->worldScale.y == Catch::Approx(3.0f));
}

TEST_CASE("SceneRenderer refreshes runtime transform mutations on every render",
          "[scene][transform]")
{
    Janus::Scene scene;
    const auto entity = scene.CreateEntity();
    const auto camera = scene.CreateEntity();

    scene.AddComponent<Janus::CameraComponent>(
        camera,
        Janus::CameraComponent{1.0f, true});

    auto* transform =
        scene.GetComponent<Janus::TransformComponent>(entity);

    transform->position = Janus::Vector2{1.0f, 2.0f};
    transform->rotationRadians = 0.25f;
    transform->scale = Janus::Vector2{2.0f, 3.0f};

    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetRegistry registry;
    Janus::AssetService assets(".", registry, *renderer);
    Janus::SceneRenderer sceneRenderer;

    REQUIRE(sceneRenderer.Render(
        scene,
        assets,
        *renderer,
        Janus::Viewport{800, 600}));
    REQUIRE_FALSE(transform->dirty);

    transform->position = Janus::Vector2{7.0f, 8.0f};
    transform->rotationRadians = 0.5f;
    transform->scale = Janus::Vector2{4.0f, 5.0f};

    REQUIRE(sceneRenderer.Render(
        scene,
        assets,
        *renderer,
        Janus::Viewport{800, 600}));

    REQUIRE(transform->worldPosition.x == Catch::Approx(7.0f));
    REQUIRE(transform->worldPosition.y == Catch::Approx(8.0f));
    REQUIRE(transform->worldRotationRadians == Catch::Approx(0.5f));
    REQUIRE(transform->worldScale.x == Catch::Approx(4.0f));
    REQUIRE(transform->worldScale.y == Catch::Approx(5.0f));
}

TEST_CASE("SceneRenderer preserves ancestor transforms when a deep child is dirty",
          "[scene][transform]")
{
    Janus::Scene scene;
    const auto root = scene.CreateEntity();
    const auto child = scene.CreateEntity();
    const auto grandchild = scene.CreateEntity();

    REQUIRE(scene.SetParent(child, root));
    REQUIRE(scene.SetParent(grandchild, child));

    const auto camera = scene.CreateEntity();
    scene.AddComponent<Janus::CameraComponent>(
        camera,
        Janus::CameraComponent{1.0f, true});

    auto* rootTransform =
        scene.GetComponent<Janus::TransformComponent>(root);
    auto* childTransform =
        scene.GetComponent<Janus::TransformComponent>(child);
    auto* grandchildTransform =
        scene.GetComponent<Janus::TransformComponent>(grandchild);

    rootTransform->position = Janus::Vector2{10.0f, 0.0f};
    childTransform->position = Janus::Vector2{5.0f, 0.0f};
    grandchildTransform->position = Janus::Vector2{1.0f, 0.0f};

    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetRegistry registry;
    Janus::AssetService assets(".", registry, *renderer);
    Janus::SceneRenderer sceneRenderer;

    REQUIRE(sceneRenderer.Render(
        scene,
        assets,
        *renderer,
        Janus::Viewport{800, 600}));
    REQUIRE(grandchildTransform->worldPosition.x == Catch::Approx(16.0f));

    grandchildTransform->position = Janus::Vector2{3.0f, 0.0f};
    grandchildTransform->dirty = true;

    REQUIRE(sceneRenderer.Render(
        scene,
        assets,
        *renderer,
        Janus::Viewport{800, 600}));

    REQUIRE(grandchildTransform->worldPosition.x == Catch::Approx(18.0f));
    REQUIRE(grandchildTransform->worldPosition.y == Catch::Approx(0.0f));
}
