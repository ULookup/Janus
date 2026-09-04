#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Input/InputState.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scripting/ScriptEngine.h"

#include "../Asset/AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace
{

Janus::AssetHandle RegisterScript(
    Janus::Test::AssetTempDirectory& temp,
    Janus::AssetRegistry& registry,
    const std::filesystem::path& relativePath,
    std::string_view source)
{
    const auto path = temp.Path() / relativePath;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    REQUIRE_FALSE(error);
    REQUIRE(Janus::FileSystem::WriteText(path, source));

    auto registered = registry.Register(
        Janus::AssetType::LuaScript,
        relativePath);
    REQUIRE(registered);
    return registered.Value();
}

std::unique_ptr<Janus::ScriptEngine> CreateEngine(
    Janus::Scene& scene,
    Janus::AssetService& assets,
    const Janus::InputState& input)
{
    auto created = Janus::ScriptEngine::Create(scene, assets, input);
    REQUIRE(created);
    return std::move(created).Value();
}

} // namespace

TEST_CASE(
    "ScriptEngine isolates per-entity state and runs lifecycle callbacks",
    "[scripting][script-engine][lifecycle]")
{
    Janus::Test::AssetTempDirectory temp;
    Janus::AssetRegistry registry;
    const Janus::AssetHandle script = RegisterScript(
        temp,
        registry,
        "Scripts/Counter.lua",
        R"lua(
local Script = {}

function Script.OnCreate(self)
    self.counter = 0
    assert(self.entity:id() ~= nil)
    assert(self.entity:name() ~= nil)
end

function Script.OnUpdate(self, dt)
    self.counter = self.counter + 1
    self.entity:set_position(self.counter, dt)
end

function Script.OnDestroy(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 10, y)
end

return Script
)lua");

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService assets(temp.Path(), registry, *renderer);
    Janus::Scene scene;
    const auto first = scene.CreateEntity("First");
    const auto second = scene.CreateEntity("Second");
    REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
        first,
        Janus::LuaScriptComponent{script, true}) != nullptr);
    REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
        second,
        Janus::LuaScriptComponent{script, true}) != nullptr);

    Janus::InputState input;
    auto engine = CreateEngine(scene, assets, input);

    REQUIRE(engine->Start());
    REQUIRE(engine->IsRunning());
    REQUIRE(engine->InstanceCount() == 2);

    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.25)));
    const auto* firstTransform =
        scene.GetComponent<Janus::TransformComponent>(first);
    const auto* secondTransform =
        scene.GetComponent<Janus::TransformComponent>(second);
    REQUIRE(firstTransform != nullptr);
    REQUIRE(secondTransform != nullptr);
    REQUIRE(firstTransform->position.x == Catch::Approx(1.0f));
    REQUIRE(secondTransform->position.x == Catch::Approx(1.0f));
    REQUIRE(firstTransform->position.y == Catch::Approx(0.25f));
    REQUIRE(secondTransform->position.y == Catch::Approx(0.25f));

    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.5)));
    REQUIRE(firstTransform->position.x == Catch::Approx(2.0f));
    REQUIRE(secondTransform->position.x == Catch::Approx(2.0f));
    REQUIRE(firstTransform->position.y == Catch::Approx(0.5f));
    REQUIRE(secondTransform->position.y == Catch::Approx(0.5f));

    REQUIRE(engine->Stop());
    REQUIRE_FALSE(engine->IsRunning());
    REQUIRE(engine->InstanceCount() == 0);
    REQUIRE(firstTransform->position.x == Catch::Approx(12.0f));
    REQUIRE(secondTransform->position.x == Catch::Approx(12.0f));

    REQUIRE(engine->Stop());
    REQUIRE(firstTransform->position.x == Catch::Approx(12.0f));
    REQUIRE(secondTransform->position.x == Catch::Approx(12.0f));
}

TEST_CASE(
    "ScriptEngine reconciles component state and stale entities safely",
    "[scripting][script-engine][reconcile]")
{
    Janus::Test::AssetTempDirectory temp;
    Janus::AssetRegistry registry;
    const Janus::AssetHandle scriptA = RegisterScript(
        temp,
        registry,
        "Scripts/A.lua",
        R"lua(
local Script = {}
function Script.OnCreate(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 1, y)
end
function Script.OnDestroy(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 10, y)
end
return Script
)lua");
    const Janus::AssetHandle scriptB = RegisterScript(
        temp,
        registry,
        "Scripts/B.lua",
        R"lua(
local Script = {}
function Script.OnCreate(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 100, y)
end
function Script.OnDestroy(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 1000, y)
end
return Script
)lua");

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService assets(temp.Path(), registry, *renderer);
    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Player");
    auto* scriptComponent = scene.AddComponent<Janus::LuaScriptComponent>(
        entity,
        Janus::LuaScriptComponent{scriptA, true});
    REQUIRE(scriptComponent != nullptr);

    const Janus::UUID entityId =
        scene.GetComponent<Janus::EntityIdentityComponent>(entity)->id;
    auto* transform = scene.GetComponent<Janus::TransformComponent>(entity);
    REQUIRE(transform != nullptr);

    Janus::InputState input;
    auto engine = CreateEngine(scene, assets, input);
    REQUIRE(engine->Start());
    REQUIRE(transform->position.x == Catch::Approx(1.0f));
    REQUIRE(engine->InstanceCount() == 1);

    scriptComponent->enabled = false;
    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.0)));
    REQUIRE(transform->position.x == Catch::Approx(11.0f));
    REQUIRE(engine->InstanceCount() == 0);

    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.0)));
    REQUIRE(transform->position.x == Catch::Approx(11.0f));

    scriptComponent->enabled = true;
    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.0)));
    REQUIRE(transform->position.x == Catch::Approx(12.0f));
    REQUIRE(engine->InstanceCount() == 1);

    scriptComponent->script = scriptB;
    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.0)));
    REQUIRE(transform->position.x == Catch::Approx(122.0f));
    REQUIRE(engine->InstanceCount() == 1);

    REQUIRE(scene.RemoveComponent<Janus::LuaScriptComponent>(entity));
    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.0)));
    REQUIRE(transform->position.x == Catch::Approx(1122.0f));
    REQUIRE(engine->InstanceCount() == 0);

    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.0)));
    REQUIRE(transform->position.x == Catch::Approx(1122.0f));

    scriptComponent = scene.AddComponent<Janus::LuaScriptComponent>(
        entity,
        Janus::LuaScriptComponent{scriptA, true});
    REQUIRE(scriptComponent != nullptr);
    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.0)));
    REQUIRE(transform->position.x == Catch::Approx(1123.0f));
    REQUIRE(engine->InstanceCount() == 1);

    REQUIRE(scene.DestroyEntity(entity));
    const auto staleResult =
        engine->Update(Janus::TimeStep::FromSeconds(0.0));
    REQUIRE_FALSE(staleResult);
    REQUIRE(staleResult.GetError().code == Janus::ErrorCode::ScriptRuntimeFailed);
    REQUIRE(staleResult.GetError().message.find("OnDestroy") != std::string::npos);
    REQUIRE(staleResult.GetError().message.find(entityId.ToString()) != std::string::npos);
    REQUIRE(staleResult.GetError().message.find(scriptA.ToString()) != std::string::npos);
    REQUIRE(staleResult.GetError().message.find("stale") != std::string::npos);
    REQUIRE(engine->InstanceCount() == 0);

    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.0)));
    REQUIRE(engine->Stop());
}

TEST_CASE(
    "ScriptEngine exposes current-frame InputState to Lua",
    "[scripting][script-engine][input]")
{
    Janus::Test::AssetTempDirectory temp;
    Janus::AssetRegistry registry;
    const Janus::AssetHandle script = RegisterScript(
        temp,
        registry,
        "Scripts/Input.lua",
        R"lua(
local Script = {}
function Script.OnUpdate(self, dt)
    local x, y = self.entity:get_position()
    if Input.is_key_down("D") then
        x = x + 1
    end
    if Input.was_key_pressed("Space") then
        y = y + 10
    end
    if Input.was_key_released("Escape") then
        y = y + 100
    end
    self.entity:set_position(x, y)
end
return Script
)lua");

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService assets(temp.Path(), registry, *renderer);
    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Player");
    REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
        entity,
        Janus::LuaScriptComponent{script, true}) != nullptr);
    auto* transform = scene.GetComponent<Janus::TransformComponent>(entity);
    REQUIRE(transform != nullptr);

    Janus::InputState input;
    auto engine = CreateEngine(scene, assets, input);
    REQUIRE(engine->Start());

    input.BeginFrame();
    input.Apply(Janus::Event{Janus::KeyPressedEvent{Janus::KeyCode::D, false}});
    input.Apply(Janus::Event{Janus::KeyPressedEvent{Janus::KeyCode::Space, false}});
    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.016)));
    REQUIRE(transform->position.x == Catch::Approx(1.0f));
    REQUIRE(transform->position.y == Catch::Approx(10.0f));

    input.BeginFrame();
    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.016)));
    REQUIRE(transform->position.x == Catch::Approx(2.0f));
    REQUIRE(transform->position.y == Catch::Approx(10.0f));

    input.BeginFrame();
    input.Apply(Janus::Event{Janus::KeyReleasedEvent{Janus::KeyCode::D}});
    input.Apply(Janus::Event{Janus::KeyReleasedEvent{Janus::KeyCode::Escape}});
    REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.016)));
    REQUIRE(transform->position.x == Catch::Approx(2.0f));
    REQUIRE(transform->position.y == Catch::Approx(110.0f));

    REQUIRE(engine->Stop());
}

TEST_CASE(
    "ScriptEngine reports binding and script contract failures with context",
    "[scripting][script-engine][errors]")
{
    Janus::Test::AssetTempDirectory temp;
    Janus::AssetRegistry registry;
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService assets(temp.Path(), registry, *renderer);
    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Player");
    const Janus::UUID entityId =
        scene.GetComponent<Janus::EntityIdentityComponent>(entity)->id;
    Janus::InputState input;

    SECTION("missing callbacks are valid")
    {
        const Janus::AssetHandle script = RegisterScript(
            temp,
            registry,
            "Scripts/Empty.lua",
            "return {}\n");
        REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
            entity,
            Janus::LuaScriptComponent{script, true}) != nullptr);

        auto engine = CreateEngine(scene, assets, input);
        REQUIRE(engine->Start());
        REQUIRE(engine->Update(Janus::TimeStep::FromSeconds(0.1)));
        REQUIRE(engine->Stop());
    }

    SECTION("script chunk must return exactly one table")
    {
        const Janus::AssetHandle script = RegisterScript(
            temp,
            registry,
            "Scripts/BadReturn.lua",
            "return {}, {}\n");
        REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
            entity,
            Janus::LuaScriptComponent{script, true}) != nullptr);

        auto engine = CreateEngine(scene, assets, input);
        const auto started = engine->Start();
        REQUIRE_FALSE(started);
        REQUIRE(started.GetError().code == Janus::ErrorCode::ScriptRuntimeFailed);
        REQUIRE(started.GetError().message.find("exactly one table") != std::string::npos);
        REQUIRE(engine->InstanceCount() == 0);
    }

    SECTION("invalid key names are Lua errors")
    {
        const Janus::AssetHandle script = RegisterScript(
            temp,
            registry,
            "Scripts/BadKey.lua",
            R"lua(
local Script = {}
function Script.OnUpdate(self, dt)
    Input.is_key_down("Mouse1")
end
return Script
)lua");
        REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
            entity,
            Janus::LuaScriptComponent{script, true}) != nullptr);

        auto engine = CreateEngine(scene, assets, input);
        REQUIRE(engine->Start());
        const auto updated = engine->Update(Janus::TimeStep::FromSeconds(0.1));
        REQUIRE_FALSE(updated);
        REQUIRE(updated.GetError().message.find("Unknown Janus key name") != std::string::npos);
        REQUIRE(updated.GetError().message.find("OnUpdate") != std::string::npos);
        REQUIRE(updated.GetError().message.find(entityId.ToString()) != std::string::npos);
        REQUIRE(updated.GetError().message.find(script.ToString()) != std::string::npos);
        REQUIRE(engine->Stop());
    }

    SECTION("Input table rejects assignment")
    {
        const Janus::AssetHandle script = RegisterScript(
            temp,
            registry,
            "Scripts/MutateInput.lua",
            R"lua(
local Script = {}
function Script.OnUpdate(self, dt)
    Input.is_key_down = nil
end
return Script
)lua");
        REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
            entity,
            Janus::LuaScriptComponent{script, true}) != nullptr);

        auto engine = CreateEngine(scene, assets, input);
        REQUIRE(engine->Start());
        const auto updated = engine->Update(Janus::TimeStep::FromSeconds(0.1));
        REQUIRE_FALSE(updated);
        REQUIRE(updated.GetError().message.find("read-only") != std::string::npos);
        REQUIRE(engine->Stop());
    }

    SECTION("missing Transform is a safe binding error")
    {
        const Janus::AssetHandle script = RegisterScript(
            temp,
            registry,
            "Scripts/NeedsTransform.lua",
            R"lua(
local Script = {}
function Script.OnUpdate(self, dt)
    self.entity:get_position()
end
return Script
)lua");
        REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
            entity,
            Janus::LuaScriptComponent{script, true}) != nullptr);

        auto engine = CreateEngine(scene, assets, input);
        REQUIRE(engine->Start());
        REQUIRE(scene.RemoveComponent<Janus::TransformComponent>(entity));
        const auto updated = engine->Update(Janus::TimeStep::FromSeconds(0.1));
        REQUIRE_FALSE(updated);
        REQUIRE(updated.GetError().message.find("TransformComponent") != std::string::npos);
        REQUIRE(updated.GetError().message.find("OnUpdate") != std::string::npos);
        REQUIRE(engine->Stop());
    }

    SECTION("callback traceback carries entity and script context")
    {
        const Janus::AssetHandle script = RegisterScript(
            temp,
            registry,
            "Scripts/Boom.lua",
            R"lua(
local Script = {}
function Script.OnUpdate(self, dt)
    error("boom")
end
return Script
)lua");
        REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
            entity,
            Janus::LuaScriptComponent{script, true}) != nullptr);

        auto engine = CreateEngine(scene, assets, input);
        REQUIRE(engine->Start());
        const auto updated = engine->Update(Janus::TimeStep::FromSeconds(0.1));
        REQUIRE_FALSE(updated);
        REQUIRE(updated.GetError().code == Janus::ErrorCode::ScriptRuntimeFailed);
        REQUIRE(updated.GetError().message.find("boom") != std::string::npos);
        REQUIRE(updated.GetError().message.find("OnUpdate") != std::string::npos);
        REQUIRE(updated.GetError().message.find(entityId.ToString()) != std::string::npos);
        REQUIRE(updated.GetError().message.find(script.ToString()) != std::string::npos);
        REQUIRE(engine->Stop());
    }
}


TEST_CASE(
    "ScriptEngine hot reload recreates only changed script instances",
    "[scripting][script-engine][reload]")
{
    Janus::Test::AssetTempDirectory temp;
    Janus::AssetRegistry registry;
    const auto relativePath = std::filesystem::path("Scripts/Reload.lua");
    const Janus::AssetHandle script = RegisterScript(
        temp,
        registry,
        relativePath,
        R"lua(
local Script = {}
function Script.OnCreate(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 1, y)
end
function Script.OnDestroy(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 10, y)
end
return Script
)lua");

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService assets(temp.Path(), registry, *renderer);
    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Reloadable");
    REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
        entity,
        Janus::LuaScriptComponent{script, true}) != nullptr);
    auto* transform = scene.GetComponent<Janus::TransformComponent>(entity);
    REQUIRE(transform != nullptr);

    Janus::InputState input;
    auto engine = CreateEngine(scene, assets, input);
    REQUIRE(engine->Start());
    REQUIRE(transform->position.x == Catch::Approx(1.0f));

    // Unchanged source must not recreate the instance.
    REQUIRE(engine->ReloadChangedScripts());
    REQUIRE(transform->position.x == Catch::Approx(1.0f));
    REQUIRE(engine->InstanceCount() == 1);

    const auto scriptPath = temp.Path() / relativePath;
    const auto initialTime = assets.GetLastWriteTime(script);
    REQUIRE(initialTime);
    REQUIRE(Janus::FileSystem::WriteText(
        scriptPath,
        R"lua(
local Script = {}
function Script.OnCreate(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 100, y)
end
function Script.OnDestroy(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 1000, y)
end
return Script
)lua"));

    std::error_code error;
    std::filesystem::last_write_time(
        scriptPath,
        initialTime.Value() + std::chrono::seconds(2),
        error);
    REQUIRE_FALSE(error);

    REQUIRE(engine->ReloadChangedScripts());
    // Old OnDestroy (+10), then new OnCreate (+100).
    REQUIRE(transform->position.x == Catch::Approx(111.0f));
    REQUIRE(engine->InstanceCount() == 1);
    REQUIRE(engine->Stop());
    REQUIRE(transform->position.x == Catch::Approx(1111.0f));
}

TEST_CASE(
    "ScriptEngine hot reload fails atomically on invalid changed source",
    "[scripting][script-engine][reload][errors]")
{
    Janus::Test::AssetTempDirectory temp;
    Janus::AssetRegistry registry;
    const auto relativePath = std::filesystem::path("Scripts/ReloadError.lua");
    const Janus::AssetHandle script = RegisterScript(
        temp,
        registry,
        relativePath,
        R"lua(
local Script = {}
function Script.OnCreate(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 1, y)
end
function Script.OnDestroy(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 10, y)
end
return Script
)lua");

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::AssetService assets(temp.Path(), registry, *renderer);
    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Reloadable");
    REQUIRE(scene.AddComponent<Janus::LuaScriptComponent>(
        entity,
        Janus::LuaScriptComponent{script, true}) != nullptr);
    auto* transform = scene.GetComponent<Janus::TransformComponent>(entity);
    REQUIRE(transform != nullptr);

    Janus::InputState input;
    auto engine = CreateEngine(scene, assets, input);
    REQUIRE(engine->Start());
    REQUIRE(transform->position.x == Catch::Approx(1.0f));

    const auto scriptPath = temp.Path() / relativePath;
    const auto initialTime = assets.GetLastWriteTime(script);
    REQUIRE(initialTime);

    REQUIRE(Janus::FileSystem::WriteText(
        scriptPath,
        "local Script = { this is invalid lua\n"));
    std::error_code error;
    std::filesystem::last_write_time(
        scriptPath,
        initialTime.Value() + std::chrono::seconds(2),
        error);
    REQUIRE_FALSE(error);

    const auto failedReload = engine->ReloadChangedScripts();
    REQUIRE_FALSE(failedReload);
    REQUIRE(failedReload.GetError().code == Janus::ErrorCode::ScriptCompileFailed);
    REQUIRE(engine->InstanceCount() == 0);
    // Old instance was deterministically torn down before the failed reload.
    REQUIRE(transform->position.x == Catch::Approx(11.0f));

    REQUIRE(Janus::FileSystem::WriteText(
        scriptPath,
        R"lua(
local Script = {}
function Script.OnCreate(self)
    local x, y = self.entity:get_position()
    self.entity:set_position(x + 100, y)
end
return Script
)lua"));
    std::filesystem::last_write_time(
        scriptPath,
        initialTime.Value() + std::chrono::seconds(4),
        error);
    REQUIRE_FALSE(error);

    REQUIRE(engine->ReloadChangedScripts());
    REQUIRE(engine->InstanceCount() == 1);
    REQUIRE(transform->position.x == Catch::Approx(111.0f));
    REQUIRE(engine->Stop());
}
