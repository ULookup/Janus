#include "Asset/AssetHandle.h"
#include "Scene/Components.h"
#include "Scene/Hierarchy.h"
#include "Scene/Scene.h"
#include "Scene/SceneCloner.h"
#include "Scene/SceneReflection.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <utility>

namespace
{
Janus::ReflectionRegistry MakeReflectionRegistry()
{
    auto result = Janus::CreateBuiltinSceneReflectionRegistry();
    if (!result)
    {
        throw std::runtime_error(result.GetError().message);
    }
    return std::move(result).Value();
}
} // namespace

TEST_CASE(
    "SceneCloner creates an independent authoring-state clone",
    "[scene][clone][v0.6]")
{
    auto reflection = MakeReflectionRegistry();
    Janus::Scene source;
    source.SetName("Authoring");

    const auto root = source.CreateEntity("Root");
    const auto child = source.CreateEntity("Child");
    REQUIRE(source.SetParent(child, root));

    auto* rootTransform =
        source.GetComponent<Janus::TransformComponent>(root);
    auto* childTransform =
        source.GetComponent<Janus::TransformComponent>(child);
    REQUIRE(rootTransform != nullptr);
    REQUIRE(childTransform != nullptr);

    rootTransform->position = {10.0f, 20.0f};
    rootTransform->rotationRadians = 0.25f;
    rootTransform->scale = {2.0f, 3.0f};

    childTransform->position = {4.0f, 5.0f};
    childTransform->worldPosition = {999.0f, 888.0f};
    childTransform->dirty = false;

    const Janus::AssetHandle texture = Janus::AssetHandle::Random();
    const Janus::AssetHandle script = Janus::AssetHandle::Random();

    REQUIRE(source.AddComponent<Janus::CameraComponent>(
        root,
        Janus::CameraComponent{2.0f, true}) != nullptr);
    REQUIRE(source.AddComponent<Janus::SpriteRendererComponent>(
        child,
        Janus::SpriteRendererComponent{
            texture,
            {32.0f, 48.0f},
            {0.5f, 0.6f, 0.7f, 0.8f},
            7,
            Janus::TextureRegion{{0.1f, 0.2f}, {0.8f, 0.9f}},
            true}) != nullptr);
    REQUIRE(source.AddComponent<Janus::LuaScriptComponent>(
        child,
        Janus::LuaScriptComponent{script, true}) != nullptr);

    const Janus::UUID rootId =
        source.GetComponent<Janus::EntityIdentityComponent>(root)->id;
    const Janus::UUID childId =
        source.GetComponent<Janus::EntityIdentityComponent>(child)->id;

    auto clonedResult = Janus::SceneCloner::Clone(source, reflection);
    REQUIRE(clonedResult);

    auto cloned = std::move(clonedResult).Value();
    REQUIRE(cloned.get() != &source);
    REQUIRE(cloned->GetMetadata().id == source.GetMetadata().id);
    REQUIRE(cloned->GetMetadata().name == "Authoring");
    REQUIRE(cloned->GetEntities().size() == source.GetEntities().size());

    const auto clonedRoot = cloned->FindEntity(rootId);
    const auto clonedChild = cloned->FindEntity(childId);
    REQUIRE(clonedRoot.IsValid());
    REQUIRE(clonedChild.IsValid());

    const auto* clonedHierarchy =
        cloned->GetComponent<Janus::HierarchyComponent>(clonedChild);
    REQUIRE(clonedHierarchy != nullptr);
    REQUIRE(clonedHierarchy->parent == clonedRoot);

    const auto* clonedRootTransform =
        cloned->GetComponent<Janus::TransformComponent>(clonedRoot);
    auto* clonedChildTransform =
        cloned->GetComponent<Janus::TransformComponent>(clonedChild);
    REQUIRE(clonedRootTransform != nullptr);
    REQUIRE(clonedChildTransform != nullptr);

    REQUIRE(clonedRootTransform->position.x == Catch::Approx(10.0f));
    REQUIRE(clonedRootTransform->position.y == Catch::Approx(20.0f));
    REQUIRE(clonedRootTransform->rotationRadians == Catch::Approx(0.25f));
    REQUIRE(clonedRootTransform->scale.x == Catch::Approx(2.0f));
    REQUIRE(clonedRootTransform->scale.y == Catch::Approx(3.0f));
    REQUIRE(clonedChildTransform->position.x == Catch::Approx(4.0f));
    REQUIRE(clonedChildTransform->position.y == Catch::Approx(5.0f));

    // World transform fields are derived runtime cache, not authoring state.
    REQUIRE(clonedChildTransform->worldPosition.x == Catch::Approx(0.0f));
    REQUIRE(clonedChildTransform->worldPosition.y == Catch::Approx(0.0f));

    const auto* clonedCamera =
        cloned->GetComponent<Janus::CameraComponent>(clonedRoot);
    const auto* clonedSprite =
        cloned->GetComponent<Janus::SpriteRendererComponent>(clonedChild);
    const auto* clonedScript =
        cloned->GetComponent<Janus::LuaScriptComponent>(clonedChild);

    REQUIRE(clonedCamera != nullptr);
    REQUIRE(clonedCamera->zoom == Catch::Approx(2.0f));
    REQUIRE(clonedCamera->primary);
    REQUIRE(clonedSprite != nullptr);
    REQUIRE(clonedSprite->texture == texture);
    REQUIRE(clonedSprite->layer == 7);
    REQUIRE(clonedScript != nullptr);
    REQUIRE(clonedScript->script == script);
    REQUIRE(clonedScript->enabled);

    clonedChildTransform->position.x = 1234.0f;
    REQUIRE(childTransform->position.x == Catch::Approx(4.0f));

    REQUIRE(cloned->DestroyEntity(clonedChild));
    REQUIRE(source.FindEntity(childId).IsValid());
}

TEST_CASE(
    "SceneCloner returns an explicit failure for unserializable authoring state",
    "[scene][clone][v0.6][errors]")
{
    auto reflection = MakeReflectionRegistry();
    Janus::Scene source;
    const auto entity = source.CreateEntity("BrokenScript");

    REQUIRE(source.AddComponent<Janus::LuaScriptComponent>(
        entity,
        Janus::LuaScriptComponent{
            Janus::AssetHandle{},
            true}) != nullptr);

    const auto cloned = Janus::SceneCloner::Clone(source, reflection);

    REQUIRE_FALSE(cloned);
    REQUIRE(cloned.GetError().code == Janus::ErrorCode::InvalidState);
}
