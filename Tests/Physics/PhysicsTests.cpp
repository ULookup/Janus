#include "../Asset/AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Core/Command/CommandBus.h"
#include "Core/FileSystem/FileSystem.h"
#include "Physics/PhysicsSystem.h"
#include "Renderer/Renderer2D.h"
#include "Runtime/RuntimeExecution.h"
#include "Runtime/RuntimeSession.h"
#include "Scene/Command/SceneCommands.h"
#include "Scene/Scene.h"
#include "Scene/SceneCloner.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneSerializer.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace
{
using namespace Janus;
UUID Body(Scene& scene, const char* type, Vector2 position, Vector2 halfSize = {0.5f, 0.5f},
          bool trigger = false)
{
    auto entity = scene.CreateEntity(type);
    scene.GetComponent<TransformComponent>(entity)->position = position;
    RigidBody2DComponent body;
    body.type = type;
    body.enabled = true;
    scene.AddComponent(entity, body);
    Collider2DComponent collider;
    collider.halfSize = halfSize;
    collider.isTrigger = trigger;
    scene.AddComponent(entity, collider);
    return scene.GetComponent<EntityIdentityComponent>(entity)->id;
}
Vector2 Position(Scene& scene, UUID id)
{
    return scene.GetComponent<TransformComponent>(scene.FindEntity(id))->position;
}
} // namespace

TEST_CASE("Physics fixed ticks fall block raycast and restart", "[physics]")
{
    Scene scene;
    const auto floor = Body(scene, "static", {0, -1}, {5, 0.5f});
    const auto box = Body(scene, "dynamic", {0, 4});
    PhysicsSystem physics(scene);
    REQUIRE(physics.Start());
    REQUIRE(physics.Advance(TimeStep::FromSeconds(1.0 / 120)));
    REQUIRE(physics.GetTickCount() == 0);
    REQUIRE(Position(scene, box).y == 4);
    REQUIRE(physics.Advance(TimeStep::FromSeconds(1.0 / 120)));
    REQUIRE(physics.GetTickCount() == 1);
    REQUIRE(Position(scene, box).y < 4);
    int collisions = 0;
    auto events = [&](const PhysicsEvent& event)
    {
        if (event.kind == PhysicsEventKind::CollisionEnter)
            ++collisions;
        return Result<void>::Success();
    };
    for (int i = 0; i < 180; ++i)
        REQUIRE(physics.Advance(TimeStep::FromSeconds(1.0 / 60), events));
    REQUIRE(collisions == 1);
    REQUIRE(Position(scene, box).y == Catch::Approx(0).margin(0.03));
    auto ray = physics.Raycast({3, 5}, {3, -5});
    REQUIRE(ray);
    REQUIRE(ray.Value().has_value());
    REQUIRE(ray.Value()->entity == floor);
    REQUIRE(ray.Value()->normal.y == Catch::Approx(1));
    REQUIRE_FALSE(physics.Raycast({0, 0}, {0, 0}));
    physics.Stop();
    REQUIRE_FALSE(physics.Advance(TimeStep::FromSeconds(1)));
    scene.GetComponent<TransformComponent>(scene.FindEntity(box))->position = {0, 4};
    REQUIRE(physics.Start());
    REQUIRE(physics.GetTickCount() == 0);
    REQUIRE(physics.GetVelocity(box).Value().y == 0);
    REQUIRE(physics.Advance(TimeStep::FromSeconds(1000)));
    REQUIRE(physics.GetTickCount() == 8);
    REQUIRE(physics.GetDroppedSeconds() > 999);
    REQUIRE_FALSE(physics.Advance(TimeStep::FromSeconds(std::numeric_limits<double>::quiet_NaN())));
}

TEST_CASE("Physics trigger callbacks defer destruction until event delivery finishes", "[physics]")
{
    Scene scene;
    const auto sensor = Body(scene, "static", {0, 0}, {2, 2}, true);
    const auto box = Body(scene, "dynamic", {0, 0});
    PhysicsSystem physics(scene);
    REQUIRE(physics.Start());
    int triggers = 0;
    REQUIRE(physics.Advance(TimeStep::FromSeconds(1.0 / 60),
                            [&](const PhysicsEvent& event)
                            {
                                REQUIRE(event.kind == PhysicsEventKind::TriggerEnter);
                                REQUIRE((event.first == box || event.second == box));
                                REQUIRE(physics.QueueDestroy(box));
                                REQUIRE(scene.FindEntity(box).IsValid());
                                ++triggers;
                                return Result<void>::Success();
                            }));
    REQUIRE(triggers == 1);
    REQUIRE_FALSE(scene.FindEntity(box).IsValid());
    REQUIRE(physics.GetBodyCount() == 1);
    REQUIRE_FALSE(physics.GetVelocity(box));
    REQUIRE(physics.Advance(TimeStep::FromSeconds(1.0 / 60)));
    REQUIRE_FALSE(physics.Raycast({0, 4}, {0, -4}).Value().has_value());
    REQUIRE(physics.Raycast({0, 4}, {0, -4}, true).Value()->entity == sensor);
}

TEST_CASE("Physics validates root unit scale and component bounds before creating world",
          "[physics]")
{
    Scene scene;
    const auto box = Body(scene, "dynamic", {0, 1});
    const auto entity = scene.FindEntity(box);
    PhysicsSystem physics(scene);
    SECTION("parent")
    {
        REQUIRE(scene.SetParent(entity, scene.CreateEntity("parent")));
    }
    SECTION("scale")
    {
        scene.GetComponent<TransformComponent>(entity)->scale.x = 2;
    }
    SECTION("geometry")
    {
        scene.GetComponent<Collider2DComponent>(entity)->halfSize.x = 0;
    }
    SECTION("type")
    {
        scene.GetComponent<RigidBody2DComponent>(entity)->type = "unknown";
    }
    REQUIRE_FALSE(physics.Start());
    REQUIRE(physics.GetBodyCount() == 0);
}

TEST_CASE("Physics trigger exits kinematic velocity impulses and teleports", "[physics]")
{
    Scene scene;
    Body(scene, "static", {0, 0}, {1, 1}, true);
    const auto box = Body(scene, "kinematic", {0, 0});
    const auto dynamic = Body(scene, "dynamic", {10, 0});
    PhysicsSystem physics(scene);
    REQUIRE(physics.Start());
    REQUIRE(physics.SetVelocity(box, {4, 0}));
    REQUIRE(physics.ApplyImpulse(dynamic, {1, 0}));
    REQUIRE(physics.GetVelocity(dynamic).Value().x > 0);
    REQUIRE_FALSE(physics.ApplyImpulse(box, {1, 0}));
    REQUIRE_FALSE(physics.SetVelocity(box, {101, 0}));
    int enters = 0, exits = 0;
    for (int i = 0; i < 60; ++i)
        REQUIRE(physics.Advance(TimeStep::FromSeconds(1.0 / 60),
                                [&](const PhysicsEvent& e)
                                {
                                    if (e.kind == PhysicsEventKind::TriggerEnter)
                                        ++enters;
                                    if (e.kind == PhysicsEventKind::TriggerExit)
                                        ++exits;
                                    return Result<void>::Success();
                                }));
    REQUIRE(enters == 1);
    REQUIRE(exits == 1);
    REQUIRE(Position(scene, box).x == Catch::Approx(4).margin(0.001));
    REQUIRE(Position(scene, box).y == 0);
    REQUIRE(physics.SetPosition(box, {0, 4}));
    REQUIRE(physics.Raycast({0, 6}, {0, 2}).Value()->entity == box);
    scene.GetComponent<TransformComponent>(scene.FindEntity(box))->position = {0, 8};
    REQUIRE(physics.Advance(TimeStep{}));
    REQUIRE(physics.Raycast({0, 10}, {0, 6}).Value()->entity == box);
    scene.GetComponent<RigidBody2DComponent>(scene.FindEntity(box))->enabled = false;
    REQUIRE_FALSE(physics.Advance(TimeStep{}));
}

TEST_CASE("Physics authoring round trips clones and supports shared Undo", "[physics][reflection]")
{
    Scene scene;
    auto id = Body(scene, "dynamic", {0, 4});
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    auto saved = SceneSerializer::Serialize(scene, registry.Value());
    REQUIRE(saved);
    CHECK(saved.Value().find("velocity") == std::string::npos);
    auto loaded = SceneDeserializer::Deserialize(saved.Value(), registry.Value());
    REQUIRE(loaded);
    auto clone = SceneCloner::Clone(*loaded.Value(), registry.Value());
    REQUIRE(clone);
    REQUIRE(
        clone.Value()->GetComponent<RigidBody2DComponent>(clone.Value()->FindEntity(id))->type ==
        "dynamic");
    SceneReflection reflection(registry.Value());
    CommandBus commands;
    const auto type = MakeComponentTypeId("Collider2D");
    REQUIRE(commands.Execute(std::make_unique<SetPropertyCommand>(
        scene, reflection, id, type, MakePropertyId("Collider2D.halfSize"), Vector2{2, 3})));
    REQUIRE(commands.Undo());
    REQUIRE(scene.GetComponent<Collider2DComponent>(scene.FindEntity(id))->halfSize.x == 0.5f);
    REQUIRE(commands.Redo());
    REQUIRE_FALSE(commands.Execute(std::make_unique<SetPropertyCommand>(
        scene, reflection, id, type, MakePropertyId("Collider2D.halfSize"), Vector2{0, 1})));
    REQUIRE(
        commands.Execute(std::make_unique<RemoveComponentCommand>(scene, reflection, id, type)));
    REQUIRE(commands.Undo());
    REQUIRE(scene.GetComponent<Collider2DComponent>(scene.FindEntity(id))->halfSize.y == 3);
}

TEST_CASE("Physics Lua events destroy safely while paused Step preserves authoring",
          "[physics][runtime]")
{
    Test::AssetTempDirectory temp;
    AssetRegistry assetsRegistry;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetService assets(temp.Path(), assetsRegistry, *renderer);
    REQUIRE(FileSystem::WriteText(temp.Path() / "physics.lua", R"(
return {
 OnCreate = function(self) self.clicks = 0 end,
 OnUpdate = function(self, dt)
   local tick, dropped = Physics.stats()
   local hit = Physics.raycast(0, 4, 0, -4, true)
   Diagnostics.publish_snapshot({tick = tick, hit = hit ~= nil, clicks = self.clicks})
 end,
 OnTriggerEnter = function(self, other)
   self.clicks = self.clicks + 1
   other:destroy()
   Diagnostics.publish_snapshot({entered = true, other = other:name()})
 end
}

)"));
    auto script = assetsRegistry.Register(AssetType::LuaScript, "physics.lua");
    REQUIRE(script);
    Scene scene;
    const auto sensor = Body(scene, "static", {0, 0}, {2, 2}, true);
    const auto box = Body(scene, "dynamic", {0, 0});
    scene.AddComponent<LuaScriptComponent>(scene.FindEntity(sensor), {script.Value(), true});
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    InputState input;
    auto runtime = RuntimeSession::Start(scene, reflection.Value(), assets, input, true);
    REQUIRE(runtime);
    REQUIRE(runtime.Value()->Update(TimeStep::FromSeconds(1)));
    REQUIRE(runtime.Value()->GetPhysics().GetTickCount() == 0);
    REQUIRE(runtime.Value()->Step());
    REQUIRE(runtime.Value()->GetPhysics().GetTickCount() == 1);
    REQUIRE_FALSE(runtime.Value()->GetScene().FindEntity(box).IsValid());
    REQUIRE(scene.FindEntity(box).IsValid());
    REQUIRE(runtime.Value()->GetSnapshot().has_value());
    REQUIRE(std::get<bool>(runtime.Value()->GetSnapshot()->fields.at("entered")));
    REQUIRE(runtime.Value()->Step());
    REQUIRE(runtime.Value()->GetPhysics().GetTickCount() == 2);
    REQUIRE(runtime.Value()->Stop());
    auto restarted = RuntimeSession::Start(scene, reflection.Value(), assets, input, true);
    REQUIRE(restarted);
    REQUIRE(restarted.Value()->GetScene().FindEntity(box).IsValid());
    REQUIRE(restarted.Value()->GetPhysics().GetTickCount() == 0);
}

TEST_CASE("Physics collision callback faults flush destruction and retain attempted-frame snapshot",
          "[physics][runtime]")
{
    Test::AssetTempDirectory temp;
    AssetRegistry registry;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetService assets(temp.Path(), registry, *renderer);
    REQUIRE(FileSystem::WriteText(temp.Path() / "fault.lua", R"(
return {
 OnCreate = function(self) self.entity:set_velocity(0, -1) end,
 OnCollisionEnter = function(self, other)
   Diagnostics.publish_snapshot({collision = other:name()})
   self.entity:destroy()
   error('collision fault')
 end
}
)"));
    auto script = registry.Register(AssetType::LuaScript, "fault.lua");
    REQUIRE(script);
    Scene scene;
    Body(scene, "static", {0, -1}, {5, 0.5f});
    const auto box = Body(scene, "dynamic", {0, 0});
    scene.AddComponent<LuaScriptComponent>(scene.FindEntity(box), {script.Value(), true});
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    InputState input;
    auto runtime = RuntimeSession::Start(scene, reflection.Value(), assets, input, true);
    REQUIRE(runtime);
    REQUIRE_FALSE(runtime.Value()->Step());
    REQUIRE(runtime.Value()->GetState() == RuntimeState::Faulted);
    REQUIRE(runtime.Value()->GetStatus().failedFrameIndex == 1);
    REQUIRE(runtime.Value()->GetStatus().partialUpdate);
    REQUIRE(runtime.Value()->GetSnapshot()->frameIndex == 1);
    REQUIRE(std::get<std::string>(runtime.Value()->GetSnapshot()->fields.at("collision")) ==
            "static");
    REQUIRE_FALSE(runtime.Value()->GetScene().FindEntity(box).IsValid());
    REQUIRE(scene.FindEntity(box).IsValid());
    REQUIRE(runtime.Value()->GetPhysics().GetBodyCount() == 1);
    REQUIRE_FALSE(runtime.Value()->Step());
    REQUIRE(runtime.Value()->Stop());
    REQUIRE_FALSE(runtime.Value()->GetSnapshot());
}

TEST_CASE("Physics partitions fixed time and bounds bodies and pending descendants", "[physics]")
{
    SECTION("frame partition and paused-step remainder")
    {
        Scene a, b;
        auto first = Body(a, "dynamic", {0, 4});
        auto second = Body(b, "dynamic", {0, 4});
        PhysicsSystem one(a), two(b);
        REQUIRE(one.Start());
        REQUIRE(two.Start());
        REQUIRE(one.Advance(TimeStep::FromSeconds(1.0 / 30)));
        for (int i = 0; i < 4; ++i)
            REQUIRE(two.Advance(TimeStep::FromSeconds(1.0 / 120)));
        REQUIRE(Position(a, first).y == Position(b, second).y);
        REQUIRE(two.Advance(TimeStep::FromSeconds(1.0 / 120)));
        auto before = two.GetTickCount();
        REQUIRE(two.Advance(TimeStep::FromSeconds(1.0 / 60)));
        REQUIRE(two.GetTickCount() == before + 1);
    }
    SECTION("huge finite duration keeps diagnostics finite and catch-up bounded")
    {
        Scene scene;
        PhysicsSystem physics(scene);
        REQUIRE(physics.Start());
        REQUIRE(physics.Advance(TimeStep::FromSeconds(std::numeric_limits<f64>::max())));
        REQUIRE(physics.Advance(TimeStep::FromSeconds(std::numeric_limits<f64>::max())));
        REQUIRE(physics.GetTickCount() == 16);
        REQUIRE(physics.GetDroppedSeconds() == 1e12);
    }
    SECTION("dense contact event overflow fails explicitly")
    {
        Scene scene;
        for (int i = 0; i < 100; ++i)
            Body(scene, "dynamic", {0, 0});
        PhysicsSystem physics(scene);
        REQUIRE(physics.Start());
        auto result = physics.Advance(TimeStep::FromSeconds(1.0 / 60));
        REQUIRE_FALSE(result);
        REQUIRE(result.GetError().message.find("4096") != std::string::npos);
    }
    SECTION("body cap leaves no partial world")
    {
        Scene scene;
        for (usize i = 0; i <= PhysicsSystem::MaxBodies; ++i)
            Body(scene, "static", {0, 0});
        PhysicsSystem physics(scene);
        REQUIRE_FALSE(physics.Start());
        REQUIRE(physics.GetBodyCount() == 0);
    }
    SECTION("parent destruction includes visual descendants even without a tick")
    {
        Scene scene;
        auto parent = Body(scene, "dynamic", {0, 4});
        auto child = scene.CreateEntity("visual");
        auto childId = scene.GetComponent<EntityIdentityComponent>(child)->id;
        REQUIRE(scene.SetParent(child, scene.FindEntity(parent)));
        PhysicsSystem physics(scene);
        REQUIRE(physics.Start());
        REQUIRE(physics.QueueDestroy(parent));
        REQUIRE(physics.IsPendingDestroy(childId));
        REQUIRE_FALSE(physics.ApplyImpulse(parent, {0, 1}));
        REQUIRE(physics.Advance(TimeStep{}));
        REQUIRE_FALSE(scene.FindEntity(childId).IsValid());
        REQUIRE(physics.GetBodyCount() == 0);
    }
}

TEST_CASE("Physics queued destruction calls Lua OnDestroy while entity is still live",
          "[physics][runtime]")
{
    Test::AssetTempDirectory temp;
    AssetRegistry registry;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetService assets(temp.Path(), registry, *renderer);
    REQUIRE(FileSystem::WriteText(temp.Path() / "destroy.lua", R"(
return {
 OnCreate = function(self) self.entity:destroy() end,
 OnUpdate = function(self) error('pending entity received Update') end,
 OnDestroy = function(self) Diagnostics.publish_snapshot({destroyed = self.entity:name()}) end
}
)"));
    auto script = registry.Register(AssetType::LuaScript, "destroy.lua");
    REQUIRE(script);
    Scene scene;
    const auto id = Body(scene, "dynamic", {0, 4});
    scene.AddComponent<LuaScriptComponent>(scene.FindEntity(id), {script.Value(), true});
    InputState input;
    auto runtime = RuntimeExecution::Create(scene, assets, input);
    REQUIRE(runtime);
    REQUIRE(runtime.Value()->Start());
    REQUIRE(runtime.Value()->Advance(TimeStep{}, input));
    REQUIRE_FALSE(scene.FindEntity(id).IsValid());
    REQUIRE(std::get<std::string>(runtime.Value()->GetSnapshot()->fields.at("destroyed")) ==
            "dynamic");
    REQUIRE(runtime.Value()->InstanceCount() == 0);
    REQUIRE(runtime.Value()->Stop());
}
