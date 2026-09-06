#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Core/FileSystem/FileSystem.h"
#include "Renderer/Renderer2D.h"
#include "Runtime/RuntimeExecution.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include "../Asset/AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <limits>

namespace
{
struct ExecutionFixture
{
    Janus::Test::AssetTempDirectory temp;
    Janus::AssetRegistry registry;
    Janus::Test::FakeRenderDevice device;
    std::unique_ptr<Janus::Renderer2D> renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService assets{temp.Path(), registry, *renderer};
    Janus::Scene scene;
    Janus::ECS::Entity entity;
    Janus::AssetHandle script;

    explicit ExecutionFixture(std::string_view source)
    {
        REQUIRE(Janus::FileSystem::WriteText(temp.Path() / "Test.lua", source));
        auto registered = registry.Register(Janus::AssetType::LuaScript, "Test.lua");
        REQUIRE(registered);
        script = registered.Value();
        const auto id = Janus::UUID::Parse("11111111-1111-4111-8111-111111111111");
        REQUIRE(id);
        auto created = scene.CreateEntityWithUUID(id.Value(), "First");
        REQUIRE(created);
        entity = created.Value();
        REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
            entity, Janus::LuaScriptComponent{script, true}));
    }

    void Rewrite(std::string_view source)
    {
        const auto path = temp.Path() / "Test.lua";
        const auto previous = std::filesystem::last_write_time(path);
        REQUIRE(Janus::FileSystem::WriteText(path, source));
        // Force a distinct file timestamp without wall-clock sleeps.
        std::filesystem::last_write_time(path, previous + std::chrono::seconds(2));
    }

    Janus::Vector2 Position() const
    {
        return scene.GetComponent<Janus::TransformComponent>(entity)->position;
    }

    std::unique_ptr<Janus::RuntimeExecution> Create(const Janus::InputState& input = {},
                                                    const Janus::InputBindings& bindings = {})
    {
        auto created = Janus::RuntimeExecution::Create(scene, assets, input, bindings);
        REQUIRE(created);
        return std::move(created).Value();
    }
};
} // namespace

TEST_CASE("Runtime execution owns input snapshots and stops before its borrowed scene dies",
          "[runtime-execution][v0.10]")
{
    ExecutionFixture fixture(R"lua(
return {
  OnCreate = function(self)
    assert(Input.is_action_down('Confirm'))
    self.entity:set_position(1, 0)
  end,
  OnUpdate = function(self, dt)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 1, Input.is_action_down('Confirm') and 10 or 0)
  end,
  OnDestroy = function(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 100, y)
  end
}
)lua");
    Janus::InputState input;
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::Space, false});
    Janus::InputBindings bindings{{"Confirm", {Janus::KeyCode::Space}}};
    auto execution = fixture.Create(input, bindings);
    input = {};
    bindings.clear();
    REQUIRE_FALSE(execution->IsRunning());
    REQUIRE_FALSE(execution->Advance(Janus::TimeStep{}, input));
    REQUIRE(execution->Start());
    REQUIRE_FALSE(execution->Start());
    REQUIRE(execution->InstanceCount() == 1);
    REQUIRE(fixture.Position().x == 1);
    REQUIRE(execution->Advance(Janus::TimeStep{}, input));
    REQUIRE(fixture.Position().x == 2);
    REQUIRE(fixture.Position().y == 0);
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::Space, false});
    REQUIRE(execution->Advance(Janus::TimeStep{}, input));
    REQUIRE(fixture.Position().x == 3);
    REQUIRE(fixture.Position().y == 10);
    execution.reset();
    REQUIRE(fixture.Position().x == 103);
}

TEST_CASE("Runtime execution reloads before update and skip keeps the running source",
          "[runtime-execution][v0.10]")
{
    ExecutionFixture fixture(R"lua(
return {
  OnCreate = function(self) self.entity:set_position(100, 0) end,
  OnUpdate = function(self, dt)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 1, y)
  end
}
)lua");
    auto execution = fixture.Create();
    REQUIRE(execution->Start());
    REQUIRE(execution->Advance(Janus::TimeStep{}, {}));
    REQUIRE(fixture.Position().x == 101);
    fixture.Rewrite(R"lua(
return {
  OnCreate = function(self) self.entity:set_position(200, 0) end,
  OnUpdate = function(self, dt)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 10, y)
  end
}
)lua");
    REQUIRE(execution->Advance(Janus::TimeStep{}, {}, Janus::ScriptReloadPolicy::Skip));
    REQUIRE(fixture.Position().x == 102);
    REQUIRE(execution->Advance(Janus::TimeStep{}, {}));
    REQUIRE(fixture.Position().x == 210);
    fixture.Rewrite("return { invalid lua");
    const auto failed = execution->Advance(Janus::TimeStep{}, {});
    REQUIRE_FALSE(failed);
    REQUIRE(failed.GetError().code == Janus::ErrorCode::ScriptCompileFailed);
    REQUIRE(fixture.Position().x == 210);
    REQUIRE(execution->Stop());
    REQUIRE(execution->InstanceCount() == 0);
}

TEST_CASE("Runtime execution rejects nonfinite time before reload or script mutation",
          "[runtime-execution][v0.10]")
{
    ExecutionFixture fixture("return { OnUpdate = function(self, dt) "
                             "self.entity:set_position(1, 0) end }");
    auto execution = fixture.Create();
    REQUIRE(execution->Start());
    fixture.Rewrite("invalid lua");
    for (const auto seconds :
         {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        const auto failed = execution->Advance(Janus::TimeStep::FromSeconds(seconds), {});
        REQUIRE_FALSE(failed);
        REQUIRE(failed.GetError().code == Janus::ErrorCode::InvalidArgument);
        REQUIRE(fixture.Position().x == 0);
        REQUIRE(execution->InstanceCount() == 1);
    }
}

TEST_CASE("Runtime execution compensates partial startup and does not repeat destruction",
          "[runtime-execution][v0.10]")
{
    ExecutionFixture fixture(R"lua(
return {
  OnCreate = function(self)
    if self.entity:name() == 'Broken' then error('startup failure') end
    self.entity:set_position(1, 0)
  end,
  OnDestroy = function(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x, y + 1)
  end
}
)lua");
    // Script startup is ordered by persistent UUID, not creation/ECS order.
    const auto brokenId = Janus::UUID::Parse("ffffffff-ffff-4fff-bfff-ffffffffffff");
    REQUIRE(brokenId);
    const auto broken = fixture.scene.CreateEntityWithUUID(brokenId.Value(), "Broken");
    REQUIRE(broken);
    REQUIRE(fixture.scene.AddComponent<Janus::LuaScriptComponent>(
        broken.Value(), Janus::LuaScriptComponent{fixture.script, true}));
    auto execution = fixture.Create();
    const auto failed = execution->Start();
    REQUIRE_FALSE(failed);
    REQUIRE(failed.GetError().code == Janus::ErrorCode::ScriptRuntimeFailed);
    REQUIRE_FALSE(execution->IsRunning());
    REQUIRE(execution->InstanceCount() == 0);
    REQUIRE(fixture.Position().x == 1);
    REQUIRE(fixture.Position().y == 1);
    REQUIRE(execution->Stop());
    execution.reset();
    REQUIRE(fixture.Position().y == 1);
}

TEST_CASE("Runtime execution stop releases all instances even when a destroy callback fails",
          "[runtime-execution][v0.10]")
{
    ExecutionFixture fixture(R"lua(
return { OnDestroy = function(self)
  local x, y = self.entity:get_position()
  self.entity:set_position(x, y + 1)
  error('destroy failure')
end }
)lua");
    const auto second = fixture.scene.CreateEntity("Second");
    REQUIRE(fixture.scene.AddComponent<Janus::LuaScriptComponent>(
        second, Janus::LuaScriptComponent{fixture.script, true}));
    auto execution = fixture.Create();
    REQUIRE(execution->Start());
    const auto stopped = execution->Stop();
    REQUIRE_FALSE(stopped);
    REQUIRE(stopped.GetError().code == Janus::ErrorCode::ScriptRuntimeFailed);
    REQUIRE(execution->InstanceCount() == 0);
    REQUIRE_FALSE(execution->IsRunning());
    REQUIRE(execution->Stop());
    REQUIRE_FALSE(execution->Advance(Janus::TimeStep{}, {}));
    execution.reset();
    REQUIRE(fixture.Position().y == 1);
    REQUIRE(fixture.scene.GetComponent<Janus::TransformComponent>(second)->position.y == 1);
}
