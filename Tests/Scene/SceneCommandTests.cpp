#include "Core/Command/CommandBus.h"
#include "Scene/Command/SceneCommands.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <utility>

namespace
{

Janus::UUID EntityId(
    Janus::Scene& scene,
    Janus::ECS::Entity entity)
{
    return scene
        .GetComponent<Janus::EntityIdentityComponent>(
            entity)
        ->id;
}

Janus::ReflectionRegistry MakeReflection()
{
    return std::move(
        Janus::CreateBuiltinSceneReflectionRegistry())
        .Value();
}

} // namespace

TEST_CASE(
    "SetPropertyCommand executes undo and redo reflected state",
    "[scene][command][reflection][v0.7]")
{
    auto registry = MakeReflection();
    Janus::SceneReflection reflection(registry);
    Janus::Scene scene;

    const auto entity = scene.CreateEntity("Player");
    const Janus::UUID id = EntityId(scene, entity);

    Janus::CommandBus bus;
    REQUIRE(bus.Execute(
        std::make_unique<Janus::SetPropertyCommand>(
            scene,
            reflection,
            id,
            Janus::SceneReflectionIds::Transform,
            Janus::SceneReflectionIds::TransformPosition,
            Janus::PropertyValue{
                Janus::Vector2{12.0f, 8.0f}})));

    const auto* transform =
        scene.GetComponent<Janus::TransformComponent>(
            entity);
    REQUIRE(transform != nullptr);
    REQUIRE(
        transform->position.x
        == Catch::Approx(12.0f));

    REQUIRE(bus.Undo());
    REQUIRE(
        transform->position.x
        == Catch::Approx(0.0f));

    REQUIRE(bus.Redo());
    REQUIRE(
        transform->position.x
        == Catch::Approx(12.0f));
}

TEST_CASE(
    "SetPropertyCommand restores Camera primary contextual delta",
    "[scene][command][reflection][v0.7]")
{
    auto registry = MakeReflection();
    Janus::SceneReflection reflection(registry);
    Janus::Scene scene;

    const auto first = scene.CreateEntity("First");
    const auto second = scene.CreateEntity("Second");
    scene.AddComponent<Janus::CameraComponent>(
        first,
        Janus::CameraComponent{1.0f, true});
    scene.AddComponent<Janus::CameraComponent>(
        second,
        Janus::CameraComponent{1.0f, false});

    const Janus::UUID secondId =
        EntityId(scene, second);

    Janus::CommandBus bus;
    REQUIRE(bus.Execute(
        std::make_unique<Janus::SetPropertyCommand>(
            scene,
            reflection,
            secondId,
            Janus::SceneReflectionIds::Camera,
            Janus::SceneReflectionIds::CameraPrimary,
            Janus::PropertyValue{true})));

    REQUIRE_FALSE(
        scene.GetComponent<Janus::CameraComponent>(
            first)->primary);
    REQUIRE(
        scene.GetComponent<Janus::CameraComponent>(
            second)->primary);

    REQUIRE(bus.Undo());
    REQUIRE(
        scene.GetComponent<Janus::CameraComponent>(
            first)->primary);
    REQUIRE_FALSE(
        scene.GetComponent<Janus::CameraComponent>(
            second)->primary);

    REQUIRE(bus.Redo());
    REQUIRE_FALSE(
        scene.GetComponent<Janus::CameraComponent>(
            first)->primary);
    REQUIRE(
        scene.GetComponent<Janus::CameraComponent>(
            second)->primary);
}

TEST_CASE(
    "AddComponentCommand is reversible",
    "[scene][command][reflection][v0.7]")
{
    auto registry = MakeReflection();
    Janus::SceneReflection reflection(registry);
    Janus::Scene scene;

    const auto entity = scene.CreateEntity("Entity");
    const Janus::UUID id = EntityId(scene, entity);

    Janus::CommandBus bus;
    REQUIRE(bus.Execute(
        std::make_unique<Janus::AddComponentCommand>(
            scene,
            reflection,
            id,
            Janus::SceneReflectionIds::SpriteRenderer)));

    REQUIRE(
        scene.HasComponent<Janus::SpriteRendererComponent>(
            entity));

    REQUIRE(bus.Undo());
    REQUIRE_FALSE(
        scene.HasComponent<Janus::SpriteRendererComponent>(
            entity));

    REQUIRE(bus.Redo());
    REQUIRE(
        scene.HasComponent<Janus::SpriteRendererComponent>(
            entity));
}

TEST_CASE(
    "RemoveComponentCommand restores reflected component authoring state",
    "[scene][command][reflection][v0.7]")
{
    auto registry = MakeReflection();
    Janus::SceneReflection reflection(registry);
    Janus::Scene scene;

    const auto entity = scene.CreateEntity("Sprite");
    const Janus::UUID id = EntityId(scene, entity);
    const Janus::AssetHandle texture =
        Janus::AssetHandle::Random();

    scene.AddComponent<Janus::SpriteRendererComponent>(
        entity,
        Janus::SpriteRendererComponent{
            texture,
            {32.0f, 48.0f},
            {0.2f, 0.3f, 0.4f, 0.5f},
            7,
            Janus::TextureRegion{
                {0.1f, 0.2f},
                {0.8f, 0.9f}},
            false});

    Janus::CommandBus bus;
    REQUIRE(bus.Execute(
        std::make_unique<Janus::RemoveComponentCommand>(
            scene,
            reflection,
            id,
            Janus::SceneReflectionIds::SpriteRenderer)));

    REQUIRE_FALSE(
        scene.HasComponent<Janus::SpriteRendererComponent>(
            entity));

    REQUIRE(bus.Undo());

    const auto* sprite =
        scene.GetComponent<Janus::SpriteRendererComponent>(
            entity);
    REQUIRE(sprite != nullptr);
    REQUIRE(sprite->texture == texture);
    REQUIRE(sprite->size.x == Catch::Approx(32.0f));
    REQUIRE(sprite->size.y == Catch::Approx(48.0f));
    REQUIRE(sprite->color.r == Catch::Approx(0.2f));
    REQUIRE(sprite->color.a == Catch::Approx(0.5f));
    REQUIRE(sprite->layer == 7);
    REQUIRE(sprite->uv.min.x == Catch::Approx(0.1f));
    REQUIRE(sprite->uv.max.y == Catch::Approx(0.9f));
    REQUIRE_FALSE(sprite->enabled);

    REQUIRE(bus.Redo());
    REQUIRE_FALSE(
        scene.HasComponent<Janus::SpriteRendererComponent>(
            entity));
}

TEST_CASE(
    "Failed reflected component command does not enter history",
    "[scene][command][reflection][v0.7]")
{
    auto registry = MakeReflection();
    Janus::SceneReflection reflection(registry);
    Janus::Scene scene;

    const auto entity = scene.CreateEntity("Entity");
    const Janus::UUID id = EntityId(scene, entity);

    Janus::CommandBus bus;

    const auto removeTransform = bus.Execute(
        std::make_unique<Janus::RemoveComponentCommand>(
            scene,
            reflection,
            id,
            Janus::SceneReflectionIds::Transform));

    REQUIRE_FALSE(removeTransform);
    REQUIRE(bus.GetHistorySize() == 0);
    REQUIRE_FALSE(bus.CanUndo());

    const auto stale = bus.Execute(
        std::make_unique<Janus::AddComponentCommand>(
            scene,
            reflection,
            Janus::UUID::Random(),
            Janus::SceneReflectionIds::Camera));

    REQUIRE_FALSE(stale);
    REQUIRE(bus.GetHistorySize() == 0);
}
