#include "EditorCamera.h"
#include "ScenePicker.h"

#include "Asset/AssetHandle.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{

Janus::ECS::Entity AddSprite(
    Janus::Scene& scene,
    const char* name,
    Janus::Vector2 position,
    Janus::Vector2 size,
    Janus::Vector2 scale,
    Janus::f32 rotation,
    Janus::i32 layer,
    bool enabled = true,
    bool validTexture = true)
{
    const auto entity = scene.CreateEntity(name);

    auto* transform =
        scene.GetComponent<Janus::TransformComponent>(entity);
    transform->worldPosition = position;
    transform->worldScale = scale;
    transform->worldRotationRadians = rotation;

    scene.AddComponent<Janus::SpriteRendererComponent>(
        entity,
        Janus::SpriteRendererComponent{
            validTexture
                ? Janus::AssetHandle::Random()
                : Janus::AssetHandle{},
            size,
            Janus::Color::White(),
            layer,
            Janus::TextureRegion::Full(),
            enabled});

    return entity;
}

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
    "ScenePicker hits translated and scaled sprite bounds",
    "[editor][picking][v0.6]")
{
    Janus::Scene scene;
    const auto sprite =
        AddSprite(
            scene,
            "Sprite",
            {100.0f, 50.0f},
            {20.0f, 10.0f},
            {2.0f, 3.0f},
            0.0f,
            0);

    const auto hit =
        Janus::Editor::PickSpriteEntity(
            scene,
            Janus::Vector2{119.0f, 64.0f});
    REQUIRE(hit.has_value());
    REQUIRE(*hit == EntityId(scene, sprite));

    const auto miss =
        Janus::Editor::PickSpriteEntity(
            scene,
            Janus::Vector2{121.0f, 66.0f});
    REQUIRE_FALSE(miss.has_value());
}

TEST_CASE(
    "ScenePicker handles rotated sprite quads",
    "[editor][picking][v0.6]")
{
    Janus::Scene scene;
    const auto sprite =
        AddSprite(
            scene,
            "Rotated",
            {0.0f, 0.0f},
            {20.0f, 4.0f},
            {1.0f, 1.0f},
            3.14159265f * 0.5f,
            0);

    const auto hit =
        Janus::Editor::PickSpriteEntity(
            scene,
            Janus::Vector2{0.0f, 9.0f});
    REQUIRE(hit.has_value());
    REQUIRE(*hit == EntityId(scene, sprite));

    const auto miss =
        Janus::Editor::PickSpriteEntity(
            scene,
            Janus::Vector2{9.0f, 0.0f});
    REQUIRE_FALSE(miss.has_value());
}

TEST_CASE(
    "ScenePicker selects highest visible sprite layer",
    "[editor][picking][v0.6]")
{
    Janus::Scene scene;
    const auto back =
        AddSprite(
            scene,
            "Back",
            {0.0f, 0.0f},
            {20.0f, 20.0f},
            {1.0f, 1.0f},
            0.0f,
            1);
    const auto front =
        AddSprite(
            scene,
            "Front",
            {0.0f, 0.0f},
            {20.0f, 20.0f},
            {1.0f, 1.0f},
            0.0f,
            5);

    const auto hit =
        Janus::Editor::PickSpriteEntity(
            scene,
            Janus::Vector2{0.0f, 0.0f});

    REQUIRE(hit.has_value());
    REQUIRE(*hit == EntityId(scene, front));
    REQUIRE(*hit != EntityId(scene, back));
}

TEST_CASE(
    "ScenePicker ignores disabled or non-renderable sprites",
    "[editor][picking][v0.6]")
{
    Janus::Scene scene;
    AddSprite(
        scene,
        "Disabled",
        {0.0f, 0.0f},
        {20.0f, 20.0f},
        {1.0f, 1.0f},
        0.0f,
        10,
        false,
        true);
    AddSprite(
        scene,
        "NoTexture",
        {0.0f, 0.0f},
        {20.0f, 20.0f},
        {1.0f, 1.0f},
        0.0f,
        20,
        true,
        false);

    REQUIRE_FALSE(
        Janus::Editor::PickSpriteEntity(
            scene,
            Janus::Vector2{0.0f, 0.0f})
            .has_value());
}

TEST_CASE(
    "ScenePicker converts viewport coordinates through EditorCamera",
    "[editor][picking][camera][v0.6]")
{
    Janus::Scene scene;
    const auto sprite =
        AddSprite(
            scene,
            "Centered",
            {0.0f, 0.0f},
            {20.0f, 20.0f},
            {1.0f, 1.0f},
            0.0f,
            0);

    Janus::Editor::EditorCamera camera;

    const auto hit =
        Janus::Editor::PickSpriteEntity(
            scene,
            camera,
            Janus::Viewport{800, 600},
            Janus::Vector2{400.0f, 300.0f});

    REQUIRE(hit.has_value());
    REQUIRE(*hit == EntityId(scene, sprite));
}
