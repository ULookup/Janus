#include "Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Scene creates persistent identity for every entity", "[scene][identity]")
{
    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Player");

    REQUIRE(scene.GetMetadata().id.IsValid());
    REQUIRE(scene.GetMetadata().name == "Scene");

    const auto* identity =
        scene.GetComponent<Janus::EntityIdentityComponent>(entity);
    REQUIRE(identity != nullptr);
    REQUIRE(identity->id.IsValid());
    REQUIRE(identity->name == "Player");
    REQUIRE(scene.FindEntity(identity->id) == entity);
    REQUIRE(scene.HasComponent<Janus::TransformComponent>(entity));
    REQUIRE(scene.HasComponent<Janus::HierarchyComponent>(entity));
}

TEST_CASE("Scene controlled UUID creation rejects invalid and duplicate IDs",
          "[scene][identity]")
{
    Janus::Scene scene;
    const Janus::UUID id = Janus::UUID::Random();

    auto first = scene.CreateEntityWithUUID(id, "First");
    REQUIRE(first);
    REQUIRE(scene.FindEntity(id) == first.Value());

    const auto duplicate = scene.CreateEntityWithUUID(id, "Duplicate");
    REQUIRE_FALSE(duplicate);
    REQUIRE(duplicate.GetError().code == Janus::ErrorCode::InvalidArgument);

    const auto nil = scene.CreateEntityWithUUID(Janus::UUID{}, "Nil");
    REQUIRE_FALSE(nil);
    REQUIRE(nil.GetError().code == Janus::ErrorCode::InvalidArgument);
}

TEST_CASE("Scene removes persistent identity lookup on recursive destroy",
          "[scene][identity]")
{
    Janus::Scene scene;
    const auto parent = scene.CreateEntity("Parent");
    const auto child = scene.CreateEntity("Child");
    REQUIRE(scene.SetParent(child, parent));

    const Janus::UUID parentId =
        scene.GetComponent<Janus::EntityIdentityComponent>(parent)->id;
    const Janus::UUID childId =
        scene.GetComponent<Janus::EntityIdentityComponent>(child)->id;

    REQUIRE(scene.DestroyEntity(parent));
    REQUIRE_FALSE(scene.FindEntity(parentId).IsValid());
    REQUIRE_FALSE(scene.FindEntity(childId).IsValid());
    REQUIRE(scene.GetEntities().empty());
}
