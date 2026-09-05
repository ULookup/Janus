#include "Scene/SceneReflection.h"

#include "Asset/AssetMetadata.h"
#include "Asset/AssetRegistry.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{

Janus::UUID EntityId(
    Janus::Scene& scene,
    Janus::ECS::Entity entity)
{
    return scene
        .GetComponent<Janus::EntityIdentityComponent>(entity)
        ->id;
}

} // namespace

TEST_CASE(
    "Builtin Scene reflection factory returns explicitly owned metadata",
    "[scene][reflection][v0.7]")
{
    auto result = Janus::CreateBuiltinSceneReflectionRegistry();
    REQUIRE(result);
    REQUIRE(result.Value().GetComponentCount() == 4);
}

TEST_CASE(
    "Builtin Scene reflection exposes authoring metadata only",
    "[scene][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(Janus::RegisterBuiltinSceneReflection(registry));
    REQUIRE(registry.GetComponentCount() == 4);

    const auto* transform =
        registry.FindComponent(
            Janus::SceneReflectionIds::Transform);
    REQUIRE(transform != nullptr);
    REQUIRE_FALSE(transform->removable);
    REQUIRE(transform->properties.size() == 3);
    REQUIRE(transform->FindProperty("position") != nullptr);
    REQUIRE(transform->FindProperty("rotation") != nullptr);
    REQUIRE(transform->FindProperty("scale") != nullptr);
    REQUIRE(transform->FindProperty("worldPosition") == nullptr);
    REQUIRE(transform->FindProperty("dirty") == nullptr);

    const auto* sprite =
        registry.FindComponent(
            Janus::SceneReflectionIds::SpriteRenderer);
    REQUIRE(sprite != nullptr);

    const auto* texture =
        sprite->FindProperty(
            Janus::SceneReflectionIds::SpriteTexture);
    REQUIRE(texture != nullptr);
    REQUIRE(
        texture->type
        == Janus::PropertyType::AssetReference);
    REQUIRE(texture->referenceConstraint == "Texture");

    const auto* uvMin =
        sprite->FindProperty(
            Janus::SceneReflectionIds::SpriteUvMin);
    REQUIRE(uvMin != nullptr);
    REQUIRE_FALSE(uvMin->visible);

    const auto* script =
        registry.FindComponent(
            Janus::SceneReflectionIds::LuaScript);
    REQUIRE(script != nullptr);
    REQUIRE(
        script->FindProperty(
            Janus::SceneReflectionIds::LuaScriptAsset)
            ->referenceConstraint
        == "LuaScript");
}

TEST_CASE(
    "Scene reflection generically mutates Transform and preserves dirty state",
    "[scene][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(Janus::RegisterBuiltinSceneReflection(registry));

    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Player");
    const Janus::UUID id = EntityId(scene, entity);

    auto* transform =
        scene.GetComponent<Janus::TransformComponent>(entity);
    REQUIRE(transform != nullptr);
    transform->dirty = false;

    Janus::SceneReflection reflection(registry);
    auto delta = reflection.ApplyPropertyMutation(
        scene,
        id,
        Janus::SceneReflectionIds::Transform,
        Janus::SceneReflectionIds::TransformPosition,
        Janus::PropertyValue{
            Janus::Vector2{10.0f, 20.0f}});

    REQUIRE(delta);
    REQUIRE(delta.Value().changes.size() == 1);
    REQUIRE(transform->position.x == 10.0f);
    REQUIRE(transform->position.y == 20.0f);
    REQUIRE(transform->dirty);

    REQUIRE(reflection.RestorePropertyMutation(
        scene,
        delta.Value(),
        Janus::PropertyMutationState::Before));
    REQUIRE(transform->position.x == 0.0f);
    REQUIRE(transform->position.y == 0.0f);

    REQUIRE(reflection.RestorePropertyMutation(
        scene,
        delta.Value(),
        Janus::PropertyMutationState::After));
    REQUIRE(transform->position.x == 10.0f);
    REQUIRE(transform->position.y == 20.0f);
}

TEST_CASE(
    "Scene reflection adds and removes optional components generically",
    "[scene][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(Janus::RegisterBuiltinSceneReflection(registry));

    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Entity");
    const Janus::UUID id = EntityId(scene, entity);

    Janus::SceneReflection reflection(registry);

    REQUIRE(reflection.AddComponent(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer));

    auto hasSprite = reflection.HasComponent(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer);
    REQUIRE(hasSprite);
    REQUIRE(hasSprite.Value());

    REQUIRE(reflection.RemoveComponent(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer));

    hasSprite = reflection.HasComponent(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer);
    REQUIRE(hasSprite);
    REQUIRE_FALSE(hasSprite.Value());

    const auto removeTransform = reflection.RemoveComponent(
        scene,
        id,
        Janus::SceneReflectionIds::Transform);
    REQUIRE_FALSE(removeTransform);

    REQUIRE(reflection.AddComponent(
        scene,
        id,
        Janus::SceneReflectionIds::LuaScript));

    const auto* script =
        scene.GetComponent<Janus::LuaScriptComponent>(entity);
    REQUIRE(script != nullptr);
    REQUIRE_FALSE(script->enabled);
    REQUIRE_FALSE(script->script.IsValid());
}

TEST_CASE(
    "Scene reflection adapters preserve Color and asset references",
    "[scene][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(Janus::RegisterBuiltinSceneReflection(registry));

    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Sprite");
    const Janus::UUID id = EntityId(scene, entity);

    Janus::SceneReflection reflection(registry);
    REQUIRE(reflection.AddComponent(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer));

    REQUIRE(reflection.RestoreProperty(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer,
        Janus::SceneReflectionIds::SpriteColor,
        Janus::PropertyValue{
            Janus::ColorValue{0.1f, 0.2f, 0.3f, 0.4f}}));

    auto color = reflection.GetProperty(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer,
        Janus::SceneReflectionIds::SpriteColor);
    REQUIRE(color);
    const auto reflectedColor =
        std::get<Janus::ColorValue>(color.Value());
    REQUIRE(reflectedColor.r == 0.1f);
    REQUIRE(reflectedColor.g == 0.2f);
    REQUIRE(reflectedColor.b == 0.3f);
    REQUIRE(reflectedColor.a == 0.4f);

    const Janus::UUID assetId = Janus::UUID::Random();
    REQUIRE(reflection.RestoreProperty(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer,
        Janus::SceneReflectionIds::SpriteTexture,
        Janus::PropertyValue{
            Janus::AssetReferenceValue{assetId}}));

    auto texture = reflection.GetProperty(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer,
        Janus::SceneReflectionIds::SpriteTexture);
    REQUIRE(texture);
    REQUIRE(
        std::get<Janus::AssetReferenceValue>(
            texture.Value()).id
        == assetId);
}

TEST_CASE(
    "Scene reflection records Camera primary cross-entity mutation delta",
    "[scene][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(Janus::RegisterBuiltinSceneReflection(registry));

    Janus::Scene scene;
    const auto first = scene.CreateEntity("FirstCamera");
    const auto second = scene.CreateEntity("SecondCamera");

    scene.AddComponent<Janus::CameraComponent>(
        first,
        Janus::CameraComponent{1.0f, true});
    scene.AddComponent<Janus::CameraComponent>(
        second,
        Janus::CameraComponent{1.0f, false});

    const Janus::UUID secondId = EntityId(scene, second);

    Janus::SceneReflection reflection(registry);
    auto delta = reflection.ApplyPropertyMutation(
        scene,
        secondId,
        Janus::SceneReflectionIds::Camera,
        Janus::SceneReflectionIds::CameraPrimary,
        Janus::PropertyValue{true});

    REQUIRE(delta);
    REQUIRE(delta.Value().changes.size() == 2);
    REQUIRE_FALSE(
        scene.GetComponent<Janus::CameraComponent>(first)
            ->primary);
    REQUIRE(
        scene.GetComponent<Janus::CameraComponent>(second)
            ->primary);

    REQUIRE(reflection.RestorePropertyMutation(
        scene,
        delta.Value(),
        Janus::PropertyMutationState::Before));
    REQUIRE(
        scene.GetComponent<Janus::CameraComponent>(first)
            ->primary);
    REQUIRE_FALSE(
        scene.GetComponent<Janus::CameraComponent>(second)
            ->primary);

    REQUIRE(reflection.RestorePropertyMutation(
        scene,
        delta.Value(),
        Janus::PropertyMutationState::After));
    REQUIRE_FALSE(
        scene.GetComponent<Janus::CameraComponent>(first)
            ->primary);
    REQUIRE(
        scene.GetComponent<Janus::CameraComponent>(second)
            ->primary);

}

TEST_CASE(
    "Scene reflection reports invalid entity component and property lookups",
    "[scene][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(Janus::RegisterBuiltinSceneReflection(registry));

    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Entity");
    const Janus::UUID id = EntityId(scene, entity);

    Janus::SceneReflection reflection(registry);

    const auto missingEntity = reflection.GetProperty(
        scene,
        Janus::UUID::Random(),
        Janus::SceneReflectionIds::Transform,
        Janus::SceneReflectionIds::TransformPosition);
    REQUIRE_FALSE(missingEntity);
    REQUIRE(
        missingEntity.GetError().code
        == Janus::ErrorCode::EntityNotFound);

    const auto unknownComponent = reflection.HasComponent(
        scene,
        id,
        Janus::MakeComponentTypeId("Unknown"));
    REQUIRE_FALSE(unknownComponent);
    REQUIRE(
        unknownComponent.GetError().code
        == Janus::ErrorCode::InvalidArgument);

    const auto unknownProperty = reflection.GetProperty(
        scene,
        id,
        Janus::SceneReflectionIds::Transform,
        Janus::MakePropertyId("Transform.unknown"));
    REQUIRE_FALSE(unknownProperty);
    REQUIRE(
        unknownProperty.GetError().code
        == Janus::ErrorCode::InvalidArgument);
}

TEST_CASE(
    "Scene reflection validates Camera and Lua authoring invariants",
    "[scene][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(Janus::RegisterBuiltinSceneReflection(registry));

    Janus::AssetRegistry assets;
    const Janus::AssetHandle scriptHandle =
        Janus::AssetHandle::Random();
    REQUIRE(assets.Register(
        Janus::AssetMetadata{
            scriptHandle,
            Janus::AssetType::LuaScript,
            "Scripts/Test.lua"}));

    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Entity");
    const Janus::UUID id = EntityId(scene, entity);

    Janus::SceneReflection reflection(registry, &assets);

    REQUIRE(reflection.AddComponent(
        scene,
        id,
        Janus::SceneReflectionIds::Camera));

    const auto invalidZoom = reflection.ApplyPropertyMutation(
        scene,
        id,
        Janus::SceneReflectionIds::Camera,
        Janus::SceneReflectionIds::CameraZoom,
        Janus::PropertyValue{Janus::f32{0.0f}});
    REQUIRE_FALSE(invalidZoom);

    REQUIRE(reflection.AddComponent(
        scene,
        id,
        Janus::SceneReflectionIds::LuaScript));

    const auto invalidEnable = reflection.ApplyPropertyMutation(
        scene,
        id,
        Janus::SceneReflectionIds::LuaScript,
        Janus::SceneReflectionIds::LuaScriptEnabled,
        Janus::PropertyValue{true});
    REQUIRE_FALSE(invalidEnable);

    REQUIRE(reflection.ApplyPropertyMutation(
        scene,
        id,
        Janus::SceneReflectionIds::LuaScript,
        Janus::SceneReflectionIds::LuaScriptAsset,
        Janus::PropertyValue{
            Janus::AssetReferenceValue{scriptHandle.id}}));

    REQUIRE(reflection.ApplyPropertyMutation(
        scene,
        id,
        Janus::SceneReflectionIds::LuaScript,
        Janus::SceneReflectionIds::LuaScriptEnabled,
        Janus::PropertyValue{true}));

    const auto* script =
        scene.GetComponent<Janus::LuaScriptComponent>(entity);
    REQUIRE(script != nullptr);
    REQUIRE(script->enabled);
    REQUIRE(script->script == scriptHandle);
}

TEST_CASE(
    "Scene reflection rejects mismatched asset reference kinds",
    "[scene][reflection][v0.7]")
{
    Janus::ReflectionRegistry registry;
    REQUIRE(Janus::RegisterBuiltinSceneReflection(registry));

    Janus::AssetRegistry assets;
    const Janus::AssetHandle scriptHandle =
        Janus::AssetHandle::Random();
    REQUIRE(assets.Register(
        Janus::AssetMetadata{
            scriptHandle,
            Janus::AssetType::LuaScript,
            "Scripts/Test.lua"}));

    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Sprite");
    const Janus::UUID id = EntityId(scene, entity);

    Janus::SceneReflection reflection(registry, &assets);
    REQUIRE(reflection.AddComponent(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer));

    const auto mismatch = reflection.ApplyPropertyMutation(
        scene,
        id,
        Janus::SceneReflectionIds::SpriteRenderer,
        Janus::SceneReflectionIds::SpriteTexture,
        Janus::PropertyValue{
            Janus::AssetReferenceValue{scriptHandle.id}});

    REQUIRE_FALSE(mismatch);
    REQUIRE(
        mismatch.GetError().code
        == Janus::ErrorCode::AssetTypeMismatch);
}
