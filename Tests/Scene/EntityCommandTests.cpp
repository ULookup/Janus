#include "Core/Command/CommandBus.h"
#include "Scene/Command/EntityCommands.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

Janus::ReflectionRegistry MakeReflection()
{
    return std::move(
        Janus::CreateBuiltinSceneReflectionRegistry())
        .Value();
}

Janus::UUID EntityId(
    Janus::Scene& scene,
    Janus::ECS::Entity entity)
{
    return scene
        .GetComponent<Janus::EntityIdentityComponent>(
            entity)
        ->id;
}

std::vector<Janus::UUID> ChildOrder(
    Janus::Scene& scene,
    Janus::ECS::Entity parent)
{
    std::vector<Janus::UUID> ids;

    const auto* hierarchy =
        scene.GetComponent<Janus::HierarchyComponent>(
            parent);
    REQUIRE(hierarchy != nullptr);

    Janus::ECS::Entity child =
        hierarchy->firstChild;

    while (child.IsValid())
    {
        ids.push_back(
            EntityId(scene, child));

        const auto* childHierarchy =
            scene.GetComponent<Janus::HierarchyComponent>(
                child);
        REQUIRE(childHierarchy != nullptr);
        child = childHierarchy->nextSibling;
    }

    return ids;
}

} // namespace

TEST_CASE(
    "CreateEntityCommand preserves persistent UUID across undo redo",
    "[scene][command][entity][v0.7]")
{
    Janus::Scene scene;
    Janus::CommandBus bus;

    const Janus::UUID id =
        Janus::UUID::Random();

    REQUIRE(bus.Execute(
        std::make_unique<Janus::CreateEntityCommand>(
            scene,
            id,
            "Created")));

    auto entity = scene.FindEntity(id);
    REQUIRE(entity.IsValid());
    REQUIRE(
        scene.GetComponent<Janus::EntityIdentityComponent>(
            entity)->name
        == "Created");

    REQUIRE(bus.Undo());
    REQUIRE_FALSE(
        scene.FindEntity(id).IsValid());

    REQUIRE(bus.Redo());
    entity = scene.FindEntity(id);
    REQUIRE(entity.IsValid());
    REQUIRE(
        scene.GetComponent<Janus::EntityIdentityComponent>(
            entity)->id
        == id);
}

TEST_CASE(
    "RenameEntityCommand restores old and new names",
    "[scene][command][entity][v0.7]")
{
    Janus::Scene scene;
    const auto entity =
        scene.CreateEntity("Before");
    const Janus::UUID id =
        EntityId(scene, entity);

    Janus::CommandBus bus;
    REQUIRE(bus.Execute(
        std::make_unique<Janus::RenameEntityCommand>(
            scene,
            id,
            "After")));

    REQUIRE(
        scene.GetComponent<Janus::EntityIdentityComponent>(
            entity)->name
        == "After");

    REQUIRE(bus.Undo());
    REQUIRE(
        scene.GetComponent<Janus::EntityIdentityComponent>(
            entity)->name
        == "Before");

    REQUIRE(bus.Redo());
    REQUIRE(
        scene.GetComponent<Janus::EntityIdentityComponent>(
            entity)->name
        == "After");
}

TEST_CASE(
    "DeleteEntityCommand restores subtree UUID components and hierarchy",
    "[scene][command][entity][v0.7]")
{
    auto registry = MakeReflection();
    Janus::SceneReflection reflection(registry);
    Janus::Scene scene;

    const auto parent =
        scene.CreateEntity("ExternalParent");
    const auto root =
        scene.CreateEntity("DeletedRoot");
    const auto child =
        scene.CreateEntity("Child");
    const auto grandchild =
        scene.CreateEntity("Grandchild");

    REQUIRE(scene.SetParent(root, parent));
    REQUIRE(scene.SetParent(child, root));
    REQUIRE(scene.SetParent(grandchild, child));

    const Janus::UUID rootId =
        EntityId(scene, root);
    const Janus::UUID childId =
        EntityId(scene, child);
    const Janus::UUID grandchildId =
        EntityId(scene, grandchild);

    auto* rootTransform =
        scene.GetComponent<Janus::TransformComponent>(
            root);
    REQUIRE(rootTransform != nullptr);
    rootTransform->position =
        {42.0f, 7.0f};

    const Janus::AssetHandle texture =
        Janus::AssetHandle::Random();

    scene.AddComponent<Janus::SpriteRendererComponent>(
        child,
        Janus::SpriteRendererComponent{
            texture,
            {32.0f, 48.0f},
            {0.2f, 0.3f, 0.4f, 0.5f},
            9,
            Janus::TextureRegion{
                {0.1f, 0.2f},
                {0.8f, 0.9f}},
            false});

    scene.AddComponent<Janus::CameraComponent>(
        grandchild,
        Janus::CameraComponent{
            2.5f,
            true});

    Janus::CommandBus bus;
    REQUIRE(bus.Execute(
        std::make_unique<Janus::DeleteEntityCommand>(
            scene,
            reflection,
            rootId)));

    REQUIRE_FALSE(
        scene.FindEntity(rootId).IsValid());
    REQUIRE_FALSE(
        scene.FindEntity(childId).IsValid());
    REQUIRE_FALSE(
        scene.FindEntity(grandchildId).IsValid());

    REQUIRE(bus.Undo());

    const auto restoredRoot =
        scene.FindEntity(rootId);
    const auto restoredChild =
        scene.FindEntity(childId);
    const auto restoredGrandchild =
        scene.FindEntity(grandchildId);

    REQUIRE(restoredRoot.IsValid());
    REQUIRE(restoredChild.IsValid());
    REQUIRE(restoredGrandchild.IsValid());

    REQUIRE(
        scene.GetComponent<Janus::HierarchyComponent>(
            restoredRoot)->parent
        == parent);
    REQUIRE(
        scene.GetComponent<Janus::HierarchyComponent>(
            restoredChild)->parent
        == restoredRoot);
    REQUIRE(
        scene.GetComponent<Janus::HierarchyComponent>(
            restoredGrandchild)->parent
        == restoredChild);

    const auto* transform =
        scene.GetComponent<Janus::TransformComponent>(
            restoredRoot);
    REQUIRE(transform != nullptr);
    REQUIRE(
        transform->position.x
        == Catch::Approx(42.0f));

    const auto* sprite =
        scene.GetComponent<Janus::SpriteRendererComponent>(
            restoredChild);
    REQUIRE(sprite != nullptr);
    REQUIRE(sprite->texture == texture);
    REQUIRE(sprite->layer == 9);
    REQUIRE_FALSE(sprite->enabled);

    const auto* camera =
        scene.GetComponent<Janus::CameraComponent>(
            restoredGrandchild);
    REQUIRE(camera != nullptr);
    REQUIRE(
        camera->zoom
        == Catch::Approx(2.5f));
    REQUIRE(camera->primary);

    REQUIRE(bus.Redo());
    REQUIRE_FALSE(
        scene.FindEntity(rootId).IsValid());
}

TEST_CASE(
    "DeleteEntityCommand restores external parent sibling order",
    "[scene][command][entity][hierarchy][v0.7]")
{
    auto registry = MakeReflection();
    Janus::SceneReflection reflection(registry);
    Janus::Scene scene;

    const auto parent =
        scene.CreateEntity("Parent");
    const auto first =
        scene.CreateEntity("First");
    const auto target =
        scene.CreateEntity("Target");
    const auto last =
        scene.CreateEntity("Last");

    // SetParent inserts at the front. Reverse calls produce
    // stable visible order: First, Target, Last.
    REQUIRE(scene.SetParent(last, parent));
    REQUIRE(scene.SetParent(target, parent));
    REQUIRE(scene.SetParent(first, parent));

    const Janus::UUID firstId =
        EntityId(scene, first);
    const Janus::UUID targetId =
        EntityId(scene, target);
    const Janus::UUID lastId =
        EntityId(scene, last);

    REQUIRE(
        ChildOrder(scene, parent)
        == std::vector<Janus::UUID>{
            firstId,
            targetId,
            lastId});

    Janus::CommandBus bus;
    REQUIRE(bus.Execute(
        std::make_unique<Janus::DeleteEntityCommand>(
            scene,
            reflection,
            targetId)));

    REQUIRE(
        ChildOrder(scene, parent)
        == std::vector<Janus::UUID>{
            firstId,
            lastId});

    REQUIRE(bus.Undo());

    REQUIRE(
        ChildOrder(scene, parent)
        == std::vector<Janus::UUID>{
            firstId,
            targetId,
            lastId});
}

TEST_CASE(
    "DeleteEntityCommand undo fails safely on UUID collision",
    "[scene][command][entity][errors][v0.7]")
{
    auto registry = MakeReflection();
    Janus::SceneReflection reflection(registry);
    Janus::Scene scene;

    const auto target =
        scene.CreateEntity("Target");
    const Janus::UUID id =
        EntityId(scene, target);

    Janus::CommandBus bus;
    REQUIRE(bus.Execute(
        std::make_unique<Janus::DeleteEntityCommand>(
            scene,
            reflection,
            id)));

    REQUIRE(
        scene.CreateEntityWithUUID(
            id,
            "Collision"));

    const Janus::usize cursor =
        bus.GetCursor();

    const auto undone =
        bus.Undo();

    REQUIRE_FALSE(undone);
    REQUIRE(bus.GetCursor() == cursor);

    const auto collision =
        scene.FindEntity(id);
    REQUIRE(collision.IsValid());
    REQUIRE(
        scene.GetComponent<Janus::EntityIdentityComponent>(
            collision)->name
        == "Collision");
}
