#include "Core/Command/CommandBus.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Scene/Command/EntityCommands.h"
#include "Scene/Scene.h"
#include "UI/UIComponents.h"

#include <catch2/catch_test_macros.hpp>

using namespace Janus;

TEST_CASE("Duplicate preserves subtree data order and immutable redo identities", "[duplicate]")
{
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    Scene scene;
    auto parent = scene.CreateEntity("Parent");
    auto tail = scene.CreateEntity("Tail");
    auto original = scene.CreateEntity("Original");
    REQUIRE(scene.SetParent(tail, parent));
    REQUIRE(scene.SetParent(original, parent));
    auto child = scene.CreateEntity("Child");
    REQUIRE(scene.SetParent(child, original));
    const UUID originalId = scene.GetComponent<EntityIdentityComponent>(original)->id;
    scene.GetComponent<TransformComponent>(child)->position = {3.0f, 7.0f};
    auto command =
        DuplicateEntityCommand::Create(scene, SceneReflection(registry.Value()), originalId);
    REQUIRE(command);
    const UUID copyId = command.Value()->GetRoot();
    REQUIRE(copyId != originalId);
    CommandBus bus;
    REQUIRE(bus.Execute(std::move(command).Value()));
    REQUIRE(bus.GetHistorySize() == 1);
    auto copy = scene.FindEntity(copyId);
    REQUIRE(scene.GetComponent<EntityIdentityComponent>(copy)->name == "Original Copy");
    REQUIRE(scene.GetComponent<HierarchyComponent>(original)->nextSibling == copy);
    REQUIRE(scene.GetComponent<HierarchyComponent>(copy)->nextSibling == tail);
    auto copyChild = scene.GetComponent<HierarchyComponent>(copy)->firstChild;
    const UUID childId = scene.GetComponent<EntityIdentityComponent>(copyChild)->id;
    REQUIRE(scene.GetComponent<EntityIdentityComponent>(copyChild)->name == "Child");
    REQUIRE(scene.GetComponent<TransformComponent>(copyChild)->position.x == 3.0f);
    REQUIRE(bus.Undo());
    REQUIRE(scene.GetComponent<HierarchyComponent>(original)->nextSibling == tail);
    REQUIRE_FALSE(scene.FindEntity(copyId).IsValid());
    scene.GetComponent<EntityIdentityComponent>(original)->name = "Changed";
    scene.GetComponent<TransformComponent>(child)->position.x = 99.0f;
    REQUIRE(bus.Redo());
    REQUIRE(scene.GetComponent<EntityIdentityComponent>(scene.FindEntity(copyId))->name ==
            "Original Copy");
    REQUIRE(scene.GetComponent<TransformComponent>(scene.FindEntity(childId))->position.x == 3.0f);
}

TEST_CASE("Duplicate rejects unique scene components and excessive hierarchy atomically",
          "[duplicate]")
{
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    Scene scene;
    auto root = scene.CreateEntity("Root");
    const UUID id = scene.GetComponent<EntityIdentityComponent>(root)->id;
    SECTION("Canvas")
    {
        REQUIRE(scene.AddComponent(root, CanvasComponent{}));
    }
    SECTION("Primary camera")
    {
        CameraComponent camera;
        camera.primary = true;
        REQUIRE(scene.AddComponent(root, camera));
    }
    SECTION("Depth")
    {
        auto parent = root;
        for (int i = 0; i < 64; ++i)
        {
            auto child = scene.CreateEntity("Child");
            REQUIRE(scene.SetParent(child, parent));
            parent = child;
        }
    }
    const auto count = scene.GetEntities().size();
    auto command = DuplicateEntityCommand::Create(scene, SceneReflection(registry.Value()), id);
    if (command)
        REQUIRE_FALSE(command.Value()->Execute());
    REQUIRE(scene.GetEntities().size() == count);
}

TEST_CASE("Subtree remapping allows only the duplicate root external parent", "[duplicate][prefab]")
{
    Scene scene;
    EntitySubtreeSnapshot snapshot;
    snapshot.root = UUID::Random();
    const UUID parent = UUID::Random();
    snapshot.entities.push_back({snapshot.root, "Root", parent, 2, {}});
    auto closed = snapshot;
    REQUIRE_FALSE(RemapEntitySubtree(scene, closed, false));
    REQUIRE(RemapEntitySubtree(scene, snapshot, true));
    REQUIRE(snapshot.entities.front().parent == parent);
    auto invalid = snapshot;
    invalid.entities.push_back({UUID::Random(), "Broken", UUID::Random(), 0, {}});
    REQUIRE_FALSE(RemapEntitySubtree(scene, invalid, true));
}

TEST_CASE("Duplicate enforces entity and transaction reservation bounds before mutation",
          "[duplicate]")
{
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    Scene scene;
    auto root = scene.CreateEntity("Root");
    const auto id = scene.GetComponent<EntityIdentityComponent>(root)->id;
    SECTION("Entity count")
    {
        for (int i = 0; i < 1024; ++i)
            REQUIRE(scene.SetParent(scene.CreateEntity("Child"), root));
        REQUIRE_FALSE(DuplicateEntityCommand::Create(scene, SceneReflection(registry.Value()), id));
        REQUIRE(scene.GetEntities().size() == 1025);
    }
    SECTION("Conservative undo reservation")
    {
        scene.GetComponent<EntityIdentityComponent>(root)->name.assign(4 * 1024 * 1024, 'a');
        auto command = DuplicateEntityCommand::Create(scene, SceneReflection(registry.Value()), id);
        REQUIRE(command);
        CommandBus bus;
        auto token = bus.BeginTransaction(CommandActor::Agent);
        REQUIRE(token);
        REQUIRE_FALSE(bus.Execute(std::move(command).Value(), CommandActor::Agent, token.Value()));
        REQUIRE(scene.GetEntities().size() == 1);
        REQUIRE(bus.GetHistorySize() == 0);
    }
}

TEST_CASE("Duplicate compensates active reflection restore failure", "[duplicate]")
{
    auto builtins = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(builtins);
    ReflectionRegistry registry;
    for (const auto* original : builtins.Value().GetComponents())
    {
        auto descriptor = *original;
        if (descriptor.id == SceneReflectionIds::Transform)
            descriptor.validator = [](const void*)
            { return Result<void>::Failure(ErrorCode::InvalidState, "Injected restore failure"); };
        REQUIRE(registry.RegisterComponent(std::move(descriptor)));
    }
    Scene scene;
    auto root = scene.CreateEntity("Root");
    auto child = scene.CreateEntity("Child");
    REQUIRE(scene.SetParent(child, root));
    const auto id = scene.GetComponent<EntityIdentityComponent>(root)->id;
    auto command = DuplicateEntityCommand::Create(scene, SceneReflection(registry), id);
    REQUIRE(command);
    CommandBus bus;
    REQUIRE_FALSE(bus.Execute(std::move(command).Value()));
    REQUIRE(scene.GetEntities().size() == 2);
    REQUIRE(scene.GetComponent<HierarchyComponent>(root)->firstChild == child);
    REQUIRE(bus.GetHistorySize() == 0);
}
