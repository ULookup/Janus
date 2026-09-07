#include "Renderer/RenderQueue.h"
#include "Scene/Command/EntityCommands.h"
#include "Scene/Scene.h"
#include "Scene/SceneCloner.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneSerializer.h"
#include "UI/UIComponents.h"
#include "UI/UILayout.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace Janus;

TEST_CASE("UI layout anchors pivots clipping and reverse hit order agree", "[ui]")
{
    Scene scene;
    const auto canvas = scene.CreateEntity("Canvas");
    scene.AddComponent<CanvasComponent>(canvas, {});
    const auto panel = scene.CreateEntity("Panel");
    REQUIRE(scene.SetParent(panel, canvas));
    UIRectComponent rect;
    rect.anchorMin = rect.anchorMax = {0.5f, 0.5f};
    rect.pivot = {0.5f, 0.5f};
    rect.size = {200, 100};
    rect.clipChildren = true;
    scene.AddComponent<UIRectComponent>(panel, rect);
    scene.AddComponent<PanelComponent>(panel, {});
    const auto child = scene.CreateEntity("Child");
    REQUIRE(scene.SetParent(child, panel));
    UIRectComponent childRect;
    childRect.offset = {150, 0};
    childRect.size = {100, 50};
    scene.AddComponent<UIRectComponent>(child, childRect);
    scene.AddComponent<PanelComponent>(child, {});
    const auto childId = scene.GetComponent<EntityIdentityComponent>(child)->id;
    auto layout = UILayout::Build(scene, {800, 600});
    REQUIRE(layout);
    REQUIRE(layout.Value().items.size() == 2);
    const auto& first = layout.Value().items[0];
    CHECK(first.rect.min.x == Catch::Approx(300));
    CHECK(first.rect.min.y == Catch::Approx(250));
    CHECK(layout.Value().items[1].visible.max.x == Catch::Approx(500));
    CHECK(layout.Value().HitTest({475, 275}) == childId);
    CHECK_FALSE(layout.Value().HitTest({525, 275}).IsValid());
    scene.GetComponent<UIRectComponent>(panel)->enabled = false;
    REQUIRE(UILayout::Build(scene, {800, 600}).Value().items.empty());
}

TEST_CASE("UI layout rejects invalid geometry and multiple canvases", "[ui]")
{
    Scene scene;
    const auto canvas = scene.CreateEntity();
    scene.AddComponent<CanvasComponent>(canvas, {});
    const auto panel = scene.CreateEntity();
    REQUIRE(scene.SetParent(panel, canvas));
    UIRectComponent rect;
    rect.anchorMax = {1, 1};
    rect.size = {-20, -40};
    scene.AddComponent<UIRectComponent>(panel, rect);
    scene.AddComponent<PanelComponent>(panel, {});
    auto layout = UILayout::Build(scene, {800, 600});
    REQUIRE(layout);
    CHECK(layout.Value().items[0].rect.max.x == 780);
    CHECK(layout.Value().items[0].rect.max.y == 560);
    CHECK_FALSE(UILayout::Build(scene, {0, 600}));
    scene.GetComponent<UIRectComponent>(panel)->size = {-1000, 0};
    CHECK_FALSE(UILayout::Build(scene, {800, 600}));
    const auto other = scene.CreateEntity();
    scene.AddComponent<CanvasComponent>(other, {});
    CHECK_FALSE(UILayout::Build(scene, {800, 600}));
}

TEST_CASE("UI components persist clone and validate through active reflection", "[ui][reflection]")
{
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    SceneReflection reflection(registry.Value());
    Scene scene;
    const auto entity = scene.CreateEntity("UI");
    const auto id = scene.GetComponent<EntityIdentityComponent>(entity)->id;
    for (auto name : {"Canvas", "UIRect", "Panel", "Image"})
        REQUIRE(reflection.AddComponent(scene, id, MakeComponentTypeId(name)));
    REQUIRE(reflection.ApplyPropertyMutation(scene, id, MakeComponentTypeId("UIRect"),
                                             MakePropertyId("UIRect.offset"), Vector2{32, 48}));
    CHECK_FALSE(reflection.ApplyPropertyMutation(scene, id, MakeComponentTypeId("UIRect"),
                                                 MakePropertyId("UIRect.pivot"), Vector2{2, 0}));
    auto serialized = SceneSerializer::Serialize(scene, registry.Value());
    REQUIRE(serialized);
    auto loaded = SceneDeserializer::Deserialize(serialized.Value(), registry.Value());
    REQUIRE(loaded);
    auto clone = SceneCloner::Clone(*loaded.Value(), registry.Value());
    REQUIRE(clone);
    const auto* rect = clone.Value()->GetComponent<UIRectComponent>(clone.Value()->FindEntity(id));
    REQUIRE(rect);
    CHECK(rect->offset.x == 32);
    CHECK(rect->offset.y == 48);
    CHECK(rect->pivot.x == 0);
    CHECK(clone.Value()->HasComponent<ImageComponent>(clone.Value()->FindEntity(id)));
}

TEST_CASE("UI ordered batches never sort alternating textures", "[ui][render]")
{
    RenderQueue queue;
    for (u32 texture : {2u, 1u, 2u, 2u})
    {
        Sprite sprite;
        sprite.texture = {texture};
        queue.Submit(sprite);
    }
    const auto batches = queue.BuildBatches(true);
    REQUIRE(batches.size() == 3);
    CHECK(batches[0].texture.value == 2);
    CHECK(batches[1].texture.value == 1);
    CHECK(batches[2].sprites.size() == 2);
}

TEST_CASE(
    "UI sibling draw order survives serialization and reparent rejects cross Canvas atomically",
    "[ui][command]")
{
    Scene scene;
    auto canvas = scene.CreateEntity();
    scene.AddComponent<CanvasComponent>(canvas, {});
    auto back = scene.CreateEntity();
    auto front = scene.CreateEntity();
    for (auto entity : {front, back})
    {
        REQUIRE(scene.SetParent(entity, canvas));
        scene.AddComponent<UIRectComponent>(entity, {});
        scene.AddComponent<PanelComponent>(entity, {});
    }
    auto id = [&](ECS::Entity entity)
    { return scene.GetComponent<EntityIdentityComponent>(entity)->id; };
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    auto saved = SceneSerializer::Serialize(scene, reflection.Value());
    REQUIRE(saved);
    auto loaded = SceneDeserializer::Deserialize(saved.Value(), reflection.Value());
    REQUIRE(loaded);
    auto layout = UILayout::Build(*loaded.Value(), {800, 600});
    REQUIRE(layout);
    REQUIRE(layout.Value().items.size() == 2);
    CHECK(layout.Value().items[0].entity == id(back));
    CHECK(layout.Value().HitTest({10, 10}) == id(front));
    ReparentEntityCommand order(scene, id(front), id(canvas), 0);
    REQUIRE(order.Execute());
    CHECK(UILayout::Build(scene, {800, 600}).Value().HitTest({10, 10}) == id(back));
    REQUIRE(order.Undo());
    auto other = scene.CreateEntity();
    scene.AddComponent<CanvasComponent>(other, {});
    ReparentEntityCommand cross(scene, id(front), id(other), 0);
    CHECK_FALSE(cross.Execute());
    CHECK(scene.GetComponent<HierarchyComponent>(front)->parent == canvas);
    ReparentEntityCommand nested(scene, id(canvas), id(back), 0);
    CHECK_FALSE(nested.Execute());
    scene.GetComponent<UIRectComponent>(front)->size.x = std::numeric_limits<f32>::infinity();
    CHECK_FALSE(ValidateUIRect(*scene.GetComponent<UIRectComponent>(front)));
}

TEST_CASE("Reparent preserves layout and original sibling order across undo redo", "[ui][command]")
{
    Scene scene;
    const auto a = scene.CreateEntity("A");
    const auto b = scene.CreateEntity("B");
    const auto child = scene.CreateEntity("child");
    const auto sibling = scene.CreateEntity("sibling");
    REQUIRE(scene.SetParent(child, a));
    REQUIRE(scene.SetParent(sibling, a));
    UIRectComponent rect;
    rect.offset = {30, 40};
    scene.AddComponent<UIRectComponent>(child, rect);
    auto id = [&](ECS::Entity e) { return scene.GetComponent<EntityIdentityComponent>(e)->id; };
    ReparentEntityCommand command(scene, id(child), id(b), 0);
    REQUIRE(command.Execute());
    CHECK(scene.GetComponent<HierarchyComponent>(child)->parent == b);
    REQUIRE(command.Undo());
    CHECK(scene.GetComponent<HierarchyComponent>(a)->firstChild == sibling);
    CHECK(scene.GetComponent<HierarchyComponent>(sibling)->nextSibling == child);
    CHECK(scene.GetComponent<UIRectComponent>(child)->offset.x == 30);
    REQUIRE(command.Redo());
    ReparentEntityCommand cycle(scene, id(b), id(child), 0);
    CHECK_FALSE(cycle.Execute());
    ReparentEntityCommand missing(scene, id(child), UUID::Random(), 0);
    CHECK_FALSE(missing.Execute());
    CHECK(scene.GetComponent<HierarchyComponent>(child)->parent == b);
}
