#include "Animation/AnimationSystem.h"
#include "Application/Application.h"
#include "Application/ApplicationClient.h"
#include "Application/ApplicationConfig.h"
#include "Application/Detail/ApplicationDependencies.h"
#include "Project/ProjectSettings.h"
#include "ProjectSession.h"

#include "Core/Event/Event.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Time/FrameClock.h"
#include "Platform/Graphics/GraphicsContext.h"
#include "Platform/Window/Window.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneSerializer.h"

#include "../Asset/AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

struct RuntimeTestState
{
    Janus::FrameClock::TimePoint now{};
    Janus::Test::FakeRenderDevice rendererDevice;
    std::vector<Janus::Event> firstPollEvents;
    std::vector<std::vector<Janus::Event>> frameEvents;
    std::function<void()> onPresent;
    Janus::usize pollCount = 0;
    Janus::i32 swapInterval = -1;
};

class FakeWindow final : public Janus::Window
{
public:
    explicit FakeWindow(RuntimeTestState& state) noexcept
        : m_State(state)
    {
    }

    void PollEvents(const EventCallback& callback) override
    {
        if (m_State.pollCount == 0)
        {
            for (const Janus::Event& event : m_State.firstPollEvents)
            {
                callback(event);
            }
        }
        if (m_State.pollCount < m_State.frameEvents.size())
        {
            for (const auto& event : m_State.frameEvents[m_State.pollCount])
                callback(event);
        }
        ++m_State.pollCount;
    }

    void SetTitle(std::string_view) override
    {
    }

    Janus::u32 GetWidth() const noexcept override
    {
        return 800;
    }

    Janus::u32 GetHeight() const noexcept override
    {
        return 600;
    }

    bool ShouldClose() const noexcept override
    {
        return false;
    }

    void RequestClose() noexcept override
    {
    }

    void* GetNativeHandle() const noexcept override
    {
        return nullptr;
    }

private:
    RuntimeTestState& m_State;
};

class FakeGraphicsContext final : public Janus::GraphicsContext
{
public:
  explicit FakeGraphicsContext(RuntimeTestState& state) : m_State(state) {}
  Janus::Result<void> MakeCurrent() override
  {
      return Janus::Result<void>::Success();
    }

    Janus::Result<void> SetSwapInterval(Janus::i32 interval) override
    {
        m_State.swapInterval = interval;
        return Janus::Result<void>::Success();
    }

    void Present() noexcept override
    {
        if (m_State.onPresent)
            m_State.onPresent();
    }

  private:
    RuntimeTestState& m_State;
};

class ProjectClient final : public Janus::ApplicationClient
{
public:
    explicit ProjectClient(
        std::optional<std::filesystem::path> roundTripPath = std::nullopt)
        : m_RoundTripPath(std::move(roundTripPath))
    {
    }

    Janus::Result<void> OnInitialize(Janus::Application& application) override
    {
        initialized = true;

        auto& scene = application.GetScene();
        sceneName = scene.GetMetadata().name;
        entityCount = scene.GetEntities().size();

        if (m_RoundTripPath.has_value())
        {
            const auto saveResult =
                Janus::SceneSerializer::Save(
                    scene,
                    application.GetReflectionRegistry(),
                    *m_RoundTripPath);
            if (!saveResult)
            {
                return Janus::Result<void>::Failure(saveResult.GetError());
            }

            auto loaded = Janus::SceneDeserializer::Load(
                *m_RoundTripPath,
                application.GetReflectionRegistry());
            if (!loaded)
            {
                return Janus::Result<void>::Failure(loaded.GetError());
            }

            roundTripSceneName = loaded.Value()->GetMetadata().name;
            roundTripEntityCount = loaded.Value()->GetEntities().size();
        }

        return Janus::Result<void>::Success();
    }

    void OnEvent(const Janus::Event&, Janus::Application&) override
    {
    }

    void OnUpdate(Janus::TimeStep, Janus::Application& application) override
    {
        application.RequestExit();
    }

    void OnShutdown(Janus::Application&) noexcept override
    {
        shutdown = true;
    }

    bool initialized = false;
    bool shutdown = false;
    std::string sceneName;
    Janus::usize entityCount = 0;
    std::string roundTripSceneName;
    Janus::usize roundTripEntityCount = 0;

private:
    std::optional<std::filesystem::path> m_RoundTripPath;
};

class LuaMovementClient final : public Janus::ApplicationClient
{
public:
    Janus::Result<void> OnInitialize(Janus::Application&) override
    {
        return Janus::Result<void>::Success();
    }

    void OnEvent(const Janus::Event&, Janus::Application&) override
    {
    }

    void OnUpdate(Janus::TimeStep, Janus::Application& application) override
    {
        ++updateCount;
        if (updateCount >= 2)
        {
            application.RequestExit();
        }
    }

    void OnShutdown(Janus::Application& application) noexcept override
    {
        auto& scene = application.GetScene();
        for (const Janus::ECS::Entity entity : scene.GetEntities())
        {
            const auto* identity =
                scene.GetComponent<Janus::EntityIdentityComponent>(entity);
            const auto* transform =
                scene.GetComponent<Janus::TransformComponent>(entity);
            if (identity != nullptr
                && transform != nullptr
                && identity->name == "Player")
            {
                playerX = transform->position.x;
                break;
            }
        }
    }

    Janus::usize updateCount = 0;
    std::optional<Janus::f32> playerX;
};

Janus::Detail::ApplicationDependencies MakeDependencies(RuntimeTestState& state)
{
    Janus::Detail::ApplicationDependencies dependencies;

    dependencies.initializePlatform = []
    {
        return Janus::Result<void>::Success();
    };

    dependencies.shutdownPlatform = []
    {
    };

    dependencies.createWindow = [&state](const Janus::WindowConfig&)
    {
        std::unique_ptr<Janus::Window> window =
            std::make_unique<FakeWindow>(state);
        return Janus::Result<std::unique_ptr<Janus::Window>>::Success(
            std::move(window));
    };

    dependencies.createGraphicsContext = [&state](Janus::Window&)
    {
        std::unique_ptr<Janus::GraphicsContext> context =
            std::make_unique<FakeGraphicsContext>(state);
        return Janus::Result<
            std::unique_ptr<Janus::GraphicsContext>>::Success(
            std::move(context));
    };

    dependencies.createRenderer2D = [&state]
    {
        return Janus::Result<std::unique_ptr<Janus::Renderer2D>>::Success(
            Janus::Detail::Renderer2DTestAccess::Create(
                state.rendererDevice));
    };

    dependencies.createScene = []
    {
        return std::make_unique<Janus::Scene>();
    };

    dependencies.now = [&state]
    {
        const auto current = state.now;
        state.now += std::chrono::milliseconds(16);
        return current;
    };

    return dependencies;
}

std::filesystem::path SandboxProjectRoot()
{
    return std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path()
        / "SandboxProject";
}

Janus::ApplicationConfig ProjectConfig(
    std::filesystem::path root,
    std::filesystem::path registry = "Config/AssetRegistry.json",
    std::filesystem::path scene = "Scenes/Battle.scene")
{
    Janus::ApplicationConfig config;
    Janus::ProjectRuntimeConfig project;
    project.root = std::move(root);
    project.assetRegistryPath = std::move(registry);
    project.startupScenePath = std::move(scene);
    config.project = std::move(project);
    return config;
}

void WriteProjectText(
    const std::filesystem::path& path,
    std::string_view text)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    REQUIRE_FALSE(error);
    REQUIRE(Janus::FileSystem::WriteText(path, text));
}

constexpr std::string_view EmptyRegistry = R"json({
  "schema": "janus.asset-registry",
  "version": 1,
  "assets": []
})json";

constexpr std::string_view MissingAssetRegistry = R"json({
  "schema": "janus.asset-registry",
  "version": 1,
  "assets": [
    {
      "handle": "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
      "type": "texture",
      "path": "Assets/missing.png"
    }
  ]
})json";

constexpr std::string_view ScriptRegistry = R"json({
  "schema": "janus.asset-registry",
  "version": 1,
  "assets": [
    {
      "handle": "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
      "type": "lua-script",
      "path": "Scripts/Test.lua"
    }
  ]
})json";

constexpr std::string_view ScriptScene = R"json({
  "schema": "janus.scene",
  "version": 1,
  "scene": {
    "id": "99999999-9999-4999-8999-999999999999",
    "name": "ScriptFailure"
  },
  "entities": [
    {
      "id": "77777777-7777-4777-8777-777777777777",
      "name": "Camera",
      "parent": null,
      "siblingOrder": 0,
      "components": {
        "Transform": {
          "position": [0.0, 0.0],
          "rotation": 0.0,
          "scale": [1.0, 1.0]
        },
        "Camera": {
          "zoom": 1.0,
          "primary": true
        }
      }
    },
    {
      "id": "88888888-8888-4888-8888-888888888888",
      "name": "Scripted",
      "parent": null,
      "siblingOrder": 0,
      "components": {
        "Transform": {
          "position": [0.0, 0.0],
          "rotation": 0.0,
          "scale": [1.0, 1.0]
        },
        "LuaScript": {
          "script": "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
          "enabled": true
        }
      }
    }
  ]
})json";

constexpr std::string_view MissingAssetScene = R"json({
  "schema": "janus.scene",
  "version": 1,
  "scene": {
    "id": "66666666-6666-4666-8666-666666666666",
    "name": "MissingAsset"
  },
  "entities": [
    {
      "id": "77777777-7777-4777-8777-777777777777",
      "name": "Camera",
      "parent": null,
      "siblingOrder": 0,
      "components": {
        "Transform": {
          "position": [0.0, 0.0],
          "rotation": 0.0,
          "scale": [1.0, 1.0]
        },
        "Camera": {
          "zoom": 1.0,
          "primary": true
        }
      }
    },
    {
      "id": "88888888-8888-4888-8888-888888888888",
      "name": "BrokenSprite",
      "parent": null,
      "siblingOrder": 0,
      "components": {
        "Transform": {
          "position": [0.0, 0.0],
          "rotation": 0.0,
          "scale": [1.0, 1.0]
        },
        "SpriteRenderer": {
          "texture": "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
          "size": [32.0, 32.0],
          "color": [1.0, 1.0, 1.0, 1.0],
          "layer": 0,
          "uvMin": [0.0, 0.0],
          "uvMax": [1.0, 1.0],
          "enabled": true
        }
      }
    }
  ]
})json";

} // namespace

namespace Janus::Detail
{

struct ApplicationTestAccess
{
    static std::unique_ptr<Application> Create(
        ApplicationConfig config,
        ApplicationDependencies dependencies)
    {
        return std::unique_ptr<Application>(
            new Application(std::move(config), std::move(dependencies)));
    }
};

} // namespace Janus::Detail

TEST_CASE("Application runs a disk-backed project vertical slice",
          "[application][project][v0.4]")
{
    RuntimeTestState state;
    Janus::Test::AssetTempDirectory temp;
    ProjectClient client(temp.Path() / "RoundTrip.scene");
    auto application = Janus::Detail::ApplicationTestAccess::Create(
        ProjectConfig(SandboxProjectRoot()),
        MakeDependencies(state));

    const auto result = application->Run(client);

    REQUIRE(result);
    REQUIRE(client.initialized);
    REQUIRE(client.shutdown);
    REQUIRE(client.sceneName == "Battle");
    REQUIRE(client.entityCount == 4);
    REQUIRE(client.roundTripSceneName == "Battle");
    REQUIRE(client.roundTripEntityCount == 4);
    REQUIRE(state.rendererDevice.createdTextures.size() == 1);
    REQUIRE(state.rendererDevice.destroyedTextures.size() == 1);
    REQUIRE_FALSE(state.rendererDevice.drawCommands.empty());
}

TEST_CASE("Application rejects a malformed disk-backed startup Scene",
          "[application][project][v0.4]")
{
    RuntimeTestState state;
    Janus::Test::AssetTempDirectory temp;
    WriteProjectText(
        temp.Path() / "Config/AssetRegistry.json",
        EmptyRegistry);
    WriteProjectText(
        temp.Path() / "Scenes/Corrupt.scene",
        "{ not valid json");

    ProjectClient client;
    auto application = Janus::Detail::ApplicationTestAccess::Create(
        ProjectConfig(
            temp.Path(),
            "Config/AssetRegistry.json",
            "Scenes/Corrupt.scene"),
        MakeDependencies(state));

    const auto result = application->Run(client);

    REQUIRE_FALSE(result);
    REQUIRE(result.GetError().code == Janus::ErrorCode::InvalidArgument);
    REQUIRE_FALSE(client.initialized);
    REQUIRE_FALSE(client.shutdown);
}

TEST_CASE("Application surfaces missing project assets as runtime failures",
          "[application][project][v0.4]")
{
    RuntimeTestState state;
    Janus::Test::AssetTempDirectory temp;
    WriteProjectText(
        temp.Path() / "Config/AssetRegistry.json",
        MissingAssetRegistry);
    WriteProjectText(
        temp.Path() / "Scenes/Battle.scene",
        MissingAssetScene);

    ProjectClient client;
    auto application = Janus::Detail::ApplicationTestAccess::Create(
        ProjectConfig(temp.Path()),
        MakeDependencies(state));

    const auto result = application->Run(client);

    REQUIRE_FALSE(result);
    REQUIRE(client.initialized);
    REQUIRE(client.shutdown);
    REQUIRE(result.GetError().message.find("missing.png")
        != std::string::npos);
    REQUIRE(state.rendererDevice.createdTextures.empty());
}


TEST_CASE("Application runs disk-backed Lua gameplay before rendering",
          "[application][project][scripting][v0.5]")
{
    RuntimeTestState state;
    state.firstPollEvents.emplace_back(
        Janus::KeyPressedEvent{Janus::KeyCode::D, false});

    LuaMovementClient client;
    auto application = Janus::Detail::ApplicationTestAccess::Create(
        ProjectConfig(SandboxProjectRoot()),
        MakeDependencies(state));

    const auto result = application->Run(client);

    REQUIRE(result);
    REQUIRE(client.updateCount == 2);
    REQUIRE(client.playerX.has_value());
    REQUIRE(*client.playerX > -80.0f);
    REQUIRE(state.rendererDevice.createdTextures.size() == 1);
    REQUIRE_FALSE(state.rendererDevice.drawCommands.empty());
}

TEST_CASE("Application surfaces disk-backed Lua compile failures",
          "[application][project][scripting][v0.5][errors]")
{
    RuntimeTestState state;
    Janus::Test::AssetTempDirectory temp;
    WriteProjectText(temp.Path() / "Config/AssetRegistry.json", ScriptRegistry);
    WriteProjectText(temp.Path() / "Scenes/Battle.scene", ScriptScene);
    WriteProjectText(
        temp.Path() / "Scripts/Test.lua",
        "local Script = { this is invalid lua\n");

    ProjectClient client;
    auto application = Janus::Detail::ApplicationTestAccess::Create(
        ProjectConfig(temp.Path()),
        MakeDependencies(state));

    const auto result = application->Run(client);

    REQUIRE_FALSE(result);
    REQUIRE(result.GetError().code == Janus::ErrorCode::ScriptCompileFailed);
    REQUIRE(client.initialized);
    REQUIRE(client.shutdown);
}

TEST_CASE("Application surfaces disk-backed Lua runtime failures cleanly",
          "[application][project][scripting][v0.5][errors]")
{
    RuntimeTestState state;
    Janus::Test::AssetTempDirectory temp;
    WriteProjectText(temp.Path() / "Config/AssetRegistry.json", ScriptRegistry);
    WriteProjectText(temp.Path() / "Scenes/Battle.scene", ScriptScene);
    WriteProjectText(
        temp.Path() / "Scripts/Test.lua",
        R"lua(
local Script = {}
function Script.OnUpdate(self, dt)
    error("runtime boom")
end
return Script
)lua");

    ProjectClient client;
    auto application = Janus::Detail::ApplicationTestAccess::Create(
        ProjectConfig(temp.Path()),
        MakeDependencies(state));

    const auto result = application->Run(client);

    REQUIRE_FALSE(result);
    REQUIRE(result.GetError().code == Janus::ErrorCode::ScriptRuntimeFailed);
    REQUIRE(result.GetError().message.find("runtime boom") != std::string::npos);
    REQUIRE(client.initialized);
    REQUIRE(client.shutdown);
}

TEST_CASE("Managed Application loads project actions window settings and frame cap",
          "[v0.10][project-settings]")
{
    RuntimeTestState state;
    Janus::Test::AssetTempDirectory temp;
    WriteProjectText(temp.Path() / "Config/AssetRegistry.json", ScriptRegistry);
    WriteProjectText(temp.Path() / "Scenes/Configured.scene", ScriptScene);
    WriteProjectText(temp.Path() / "Scripts/Test.lua", R"lua(
return { OnUpdate = function(self, dt)
    assert(Input.is_action_down('Confirm'))
    assert(Input.was_action_pressed('Confirm'))
end }
)lua");
    Janus::ProjectSettings settings;
    settings.defaultScene = "Scenes/Configured.scene";
    settings.width = 960;
    settings.height = 540;
    settings.vsync = false;
    settings.targetFps = 30;
    settings.inputBindings["Confirm"] = {Janus::KeyCode::Space};
    REQUIRE(Janus::SaveProjectSettings(temp.Path(), settings));
    state.firstPollEvents.push_back(Janus::KeyPressedEvent{Janus::KeyCode::Space, false});
    auto dependencies = MakeDependencies(state);
    auto createWindow = dependencies.createWindow;
    bool configuredWindow = false, slept = false;
    dependencies.createWindow = [&](const Janus::WindowConfig& window)
    {
        configuredWindow = window.width == 960 && window.height == 540;
        return createWindow(window);
    };
    dependencies.sleepUntil = [&](Janus::FrameClock::TimePoint deadline)
    {
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline.time_since_epoch())
                .count();
        REQUIRE(milliseconds == 33);
        slept = true;
    };
    Janus::ApplicationConfig config;
    config.project = Janus::ProjectRuntimeConfig{temp.Path()};
    ProjectClient client;
    auto app = Janus::Detail::ApplicationTestAccess::Create(config, std::move(dependencies));
    REQUIRE(app->Run(client));
    REQUIRE(configuredWindow);
    REQUIRE(state.swapInterval == 0);
    REQUIRE(slept);
}

namespace
{
Janus::Vector2 ScriptedPosition(const Janus::Scene& scene)
{
    for (const auto entity : scene.GetEntities())
    {
        if (scene.GetComponent<Janus::EntityIdentityComponent>(entity)->name == "Scripted")
            return scene.GetComponent<Janus::TransformComponent>(entity)->position;
    }
    return {};
}

class RuntimeProbeClient final : public Janus::ApplicationClient
{
  public:
    Janus::Result<void> OnInitialize(Janus::Application& application) override
    {
        scene = &application.GetScene();
        initial = ScriptedPosition(*scene);
        return Janus::Result<void>::Success();
    }
    void OnUpdate(Janus::TimeStep step, Janus::Application& application) override
    {
        sameScene = sameScene && scene == &application.GetScene();
        beforeUpdate.push_back(ScriptedPosition(*scene));
        steps.push_back(step);
        if (steps.size() == 4)
            application.RequestExit();
    }
    void OnShutdown(Janus::Application& application) noexcept override
    {
        sameScene = sameScene && scene == &application.GetScene();
        shutdown = ScriptedPosition(application.GetScene());
        ++shutdownCount;
        if (beforeShutdown)
            beforeShutdown(application.GetScene());
    }
    Janus::Scene* scene = nullptr;
    Janus::Vector2 initial, shutdown;
    std::vector<Janus::Vector2> beforeUpdate, presented;
    std::vector<Janus::TimeStep> steps;
    bool sameScene = true;
    int shutdownCount = 0;
    std::function<void(Janus::Scene&)> beforeShutdown;
};

void WriteRuntimeProject(const std::filesystem::path& root, std::string_view script)
{
    WriteProjectText(root / "Config/AssetRegistry.json", ScriptRegistry);
    WriteProjectText(root / "Scenes/Battle.scene", ScriptScene);
    WriteProjectText(root / "Scripts/Test.lua", script);
    Janus::ProjectSettings settings;
    settings.inputBindings["Confirm"] = {Janus::KeyCode::Space, Janus::KeyCode::Enter};
    REQUIRE(Janus::SaveProjectSettings(root, settings));
}
} // namespace

TEST_CASE("Managed Application and Editor runtime agree on every input frame and timestep",
          "[runtime-execution][application][runtime-session][v0.10]")
{
    Janus::Test::AssetTempDirectory temp;
    WriteRuntimeProject(temp.Path(), R"lua(
return {
  OnCreate = function(self) self.entity:set_position(10, 0) end,
  OnUpdate = function(self, dt)
    local x, y = self.entity:get_position()
    if Input.is_action_down('Confirm') then x = x + 1 end
    if Input.was_action_pressed('Confirm') then x = x + 10 end
    if Input.was_action_released('Confirm') then x = x + 100 end
    local px, py = Input.pointer_position()
    if px then x = x + px end
    if Input.is_pointer_button_down('Left') then x = x + 1000 end
    self.entity:set_position(x, y + dt)
  end
}
)lua");
    RuntimeTestState state;
    state.frameEvents = {{Janus::KeyPressedEvent{Janus::KeyCode::Space, false},
                          Janus::PointerMovedEvent{{2, 3}},
                          Janus::PointerButtonPressedEvent{Janus::PointerButton::Left}},
                         {Janus::KeyPressedEvent{Janus::KeyCode::Space, true}},
                         {Janus::KeyReleasedEvent{Janus::KeyCode::Space},
                          Janus::KeyPressedEvent{Janus::KeyCode::Enter, false}},
                         {Janus::WindowFocusLostEvent{}}};
    RuntimeProbeClient client;
    state.onPresent = [&] { client.presented.push_back(ScriptedPosition(*client.scene)); };
    auto app = Janus::Detail::ApplicationTestAccess::Create(ProjectConfig(temp.Path()),
                                                            MakeDependencies(state));
    REQUIRE(app->Run(client));
    REQUIRE(client.sameScene);
    REQUIRE(client.initial.x == 0);
    REQUIRE(client.beforeUpdate.size() == 4);
    REQUIRE(client.beforeUpdate.front().x == 10);
    REQUIRE(client.presented.size() == 4);
    REQUIRE(client.shutdownCount == 1);
    REQUIRE(client.shutdown.x == client.presented.back().x);
    REQUIRE(client.steps[0].GetSeconds() == 0);
    REQUIRE(client.steps[1].GetSeconds() == Catch::Approx(0.016));

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened =
        Janus::Editor::ProjectSession::Open(Janus::ProjectRuntimeConfig{temp.Path()}, *renderer);
    REQUIRE(opened);
    auto project = std::move(opened).Value();
    Janus::InputState input, physical;
    REQUIRE(project->StartRuntime(input));
    const auto runtimeId = project->GetRuntimeStatus().runtimeId;
    for (Janus::usize i = 0; i < state.frameEvents.size(); ++i)
    {
        physical.BeginFrame();
        for (const auto& event : state.frameEvents[i])
            physical.Apply(event);
        // The fake window is 800 x 600; both hosts must supply project logical coordinates.
        input = physical.MapToViewport({0, 0}, {800, 600}, {1280, 720});
        REQUIRE(project->UpdateRuntime(client.steps[i]));
        const auto position = ScriptedPosition(project->GetRuntimeSession()->GetScene());
        REQUIRE(position.x == Catch::Approx(client.presented[i].x));
        REQUIRE(position.y == Catch::Approx(client.presented[i].y));
        REQUIRE(project->GetRuntimeStatus().frameIndex == i + 1);
        REQUIRE(project->GetRuntimeStatus().runtimeId == runtimeId);
        if (i + 1 < client.beforeUpdate.size())
            REQUIRE(client.beforeUpdate[i + 1].x == client.presented[i].x);
    }
    REQUIRE(client.presented[0].x == Catch::Approx(1024.2));
    REQUIRE(client.presented[1].x == Catch::Approx(2028.4));
    REQUIRE(client.presented[2].x == Catch::Approx(3032.6));
    REQUIRE(client.presented[3].x == Catch::Approx(3132.6));
    REQUIRE(ScriptedPosition(project->GetEditorScene()).x == 0);
    REQUIRE(project->StopRuntime());
    REQUIRE(ScriptedPosition(project->GetEditorScene()).x == 0);
}

TEST_CASE("Runtime hosts preserve their distinct failure and scene lifetime contracts",
          "[runtime-execution][application][runtime-session][v0.10]")
{
    Janus::Test::AssetTempDirectory temp;
    WriteRuntimeProject(temp.Path(), R"lua(
return {
  OnCreate = function(self) self.entity:set_position(5, 0) end,
  OnUpdate = function(self, dt)
    self.entity:set_position(25, 0)
    error('shared runtime failure')
  end
}
)lua");
    RuntimeTestState state;
    RuntimeProbeClient client;
    auto app = Janus::Detail::ApplicationTestAccess::Create(ProjectConfig(temp.Path()),
                                                            MakeDependencies(state));
    const auto failed = app->Run(client);
    REQUIRE_FALSE(failed);
    REQUIRE(failed.GetError().code == Janus::ErrorCode::ScriptRuntimeFailed);
    REQUIRE(client.shutdownCount == 1);
    REQUIRE(client.shutdown.x == 25);
    REQUIRE(client.steps.size() == 1);
    REQUIRE(client.sameScene);

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened =
        Janus::Editor::ProjectSession::Open(Janus::ProjectRuntimeConfig{temp.Path()}, *renderer);
    REQUIRE(opened);
    auto project = std::move(opened).Value();
    Janus::InputState input;
    REQUIRE(project->StartRuntime(input));
    const auto editorFailed = project->UpdateRuntime(client.steps.front());
    REQUIRE_FALSE(editorFailed);
    REQUIRE(editorFailed.GetError().code == failed.GetError().code);
    REQUIRE(editorFailed.GetError().message.find("shared runtime failure") != std::string::npos);
    const auto status = project->GetRuntimeStatus();
    REQUIRE(status.state == Janus::RuntimeState::Faulted);
    REQUIRE(status.frameIndex == 0);
    REQUIRE(status.failedFrameIndex == 1);
    REQUIRE(status.partialUpdate);
    REQUIRE(ScriptedPosition(project->GetRuntimeSession()->GetScene()).x == 25);
    REQUIRE(ScriptedPosition(project->GetEditorScene()).x == 0);
    REQUIRE_FALSE(project->SaveCurrentScene());
    REQUIRE(project->StopRuntime());
}

TEST_CASE("Managed shutdown calls the client before scripts and logs destroy errors exactly once",
          "[runtime-execution][application][v0.10]")
{
    Janus::Test::AssetTempDirectory temp;
    WriteRuntimeProject(temp.Path(), R"lua(
return { OnDestroy = function(self) error('destroy saw ' .. self.entity:name()) end }
)lua");
    RuntimeTestState state;
    RuntimeProbeClient client;
    client.beforeShutdown = [](Janus::Scene& scene)
    {
        for (const auto entity : scene.GetEntities())
        {
            auto* identity = scene.GetComponent<Janus::EntityIdentityComponent>(entity);
            if (identity->name == "Scripted")
                identity->name = "client shutdown marker";
        }
    };
    auto app = Janus::Detail::ApplicationTestAccess::Create(ProjectConfig(temp.Path()),
                                                            MakeDependencies(state));
    auto logs = app->GetLogStore();
    REQUIRE(app->Run(client));
    REQUIRE(client.shutdownCount == 1);
    app.reset();
    Janus::LogQuery query;
    query.level = Janus::LogLevel::Error;
    auto errors = logs->Read(query);
    REQUIRE(errors);
    REQUIRE(errors.Value().entries.size() == 1);
    REQUIRE(errors.Value().entries.front().message.find("destroy saw client shutdown marker") !=
            std::string::npos);
}

TEST_CASE("Paused host skips reload and neutralizes input then restart refreshes cached scripts",
          "[runtime-execution][runtime-session][v0.10]")
{
    Janus::Test::AssetTempDirectory temp;
    WriteRuntimeProject(temp.Path(), R"lua(
return { OnUpdate = function(self, dt)
  assert(not Input.is_action_down('Confirm'))
  assert(Input.pointer_position() == nil)
  assert(not Input.is_pointer_button_down('Left'))
  local x, y = self.entity:get_position()
  self.entity:set_position(x + 1, y + dt)
end }
)lua");
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened =
        Janus::Editor::ProjectSession::Open(Janus::ProjectRuntimeConfig{temp.Path()}, *renderer);
    REQUIRE(opened);
    auto project = std::move(opened).Value();
    Janus::InputState input;
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::Space, false});
    input.Apply(Janus::PointerMovedEvent{{2, 3}});
    input.Apply(Janus::PointerButtonPressedEvent{Janus::PointerButton::Left});
    REQUIRE(project->StartRuntime(input, true));
    const auto id = project->GetRuntimeStatus().runtimeId;
    const auto path = temp.Path() / "Scripts/Test.lua";
    const auto oldTime = std::filesystem::last_write_time(path);
    WriteProjectText(path, R"lua(
return { OnCreate = function(self) self.entity:set_position(50, 0) end,
  OnUpdate = function(self, dt) self.entity:set_position(75, dt) end }
)lua");
    std::filesystem::last_write_time(path, oldTime + std::chrono::seconds(2));
    REQUIRE(project->UpdateRuntime(Janus::TimeStep::FromSeconds(10)));
    REQUIRE(project->GetRuntimeStatus().frameIndex == 0);
    REQUIRE(project->StepRuntime());
    REQUIRE(project->GetRuntimeStatus().state == Janus::RuntimeState::Paused);
    REQUIRE(project->GetRuntimeStatus().runtimeId == id);
    REQUIRE(ScriptedPosition(project->GetRuntimeSession()->GetScene()).x == 1);
    REQUIRE(project->GetRuntimeStatus().simulationTimeSeconds == Catch::Approx(1.0 / 60.0));
    REQUIRE(project->StopRuntime());
    REQUIRE(project->StartRuntime(input, true));
    REQUIRE(project->GetRuntimeStatus().runtimeId != id);
    REQUIRE(ScriptedPosition(project->GetRuntimeSession()->GetScene()).x == 50);
    REQUIRE(project->StepRuntime());
    REQUIRE(ScriptedPosition(project->GetRuntimeSession()->GetScene()).x == 75);
    REQUIRE(ScriptedPosition(project->GetEditorScene()).x == 0);
}

TEST_CASE(
    "Managed Application and Editor combat produce identical published results per input frame",
    "[combat][snapshot][application][v0.10]")
{
    RuntimeTestState state;
    const std::vector<Janus::Vector2> clicks = {{180, 245}, {180, 390}, {940, 530}, {180, 390},
                                                {940, 530}, {180, 390}, {940, 530}, {940, 245},
                                                {180, 245}, {940, 390}, {940, 530}, {940, 390},
                                                {940, 530}, {940, 390}, {940, 530}, {940, 245}};
    // FakeWindow is 800x600; the Application maps it to 1280x720 logical coordinates.
    for (const auto point : clicks)
        state.frameEvents.push_back(
            {Janus::PointerMovedEvent{{point.x * 800 / 1280, point.y * 600 / 720}},
             Janus::PointerButtonPressedEvent{Janus::PointerButton::Left},
             Janus::PointerButtonReleasedEvent{Janus::PointerButton::Left}});
    class CombatClient final : public Janus::ApplicationClient
    {
      public:
        Janus::Result<void> OnInitialize(Janus::Application&) override
        {
            return Janus::Result<void>::Success();
        }
        void OnUpdate(Janus::TimeStep step, Janus::Application&) override
        {
            steps.push_back(step);
        }
        std::vector<Janus::TimeStep> steps;
    } client;
    const auto root = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "Game";
    Janus::ApplicationConfig config;
    config.project = Janus::ProjectRuntimeConfig{root};
    auto app = Janus::Detail::ApplicationTestAccess::Create(config, MakeDependencies(state));
    std::vector<Janus::ScriptSnapshot> snapshots;
    state.onPresent = [&]
    {
        REQUIRE(app->GetSnapshot());
        snapshots.push_back(*app->GetSnapshot());
        if (snapshots.size() == clicks.size())
            app->RequestExit();
    };
    const auto result = app->Run(client);
    INFO((result ? "success" : result.GetError().message));
    REQUIRE(result);
    REQUIRE_FALSE(app->GetSnapshot());
    REQUIRE(snapshots.size() == clicks.size());
    REQUIRE(std::get<std::string>(snapshots[6].fields.at("phase")) == "victory");
    REQUIRE(std::get<std::string>(snapshots[14].fields.at("phase")) == "defeat");
    REQUIRE(std::get<std::string>(snapshots[15].fields.at("phase")) == "menu");

    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open(Janus::ProjectRuntimeConfig{root}, *renderer);
    REQUIRE(opened);
    auto project = std::move(opened).Value();
    Janus::InputState input;
    REQUIRE(project->StartRuntime(input));
    for (Janus::usize i = 0; i < clicks.size(); ++i)
    {
        input.BeginFrame();
        input.Apply(Janus::PointerMovedEvent{clicks[i]});
        input.Apply(Janus::PointerButtonPressedEvent{Janus::PointerButton::Left});
        input.Apply(Janus::PointerButtonReleasedEvent{Janus::PointerButton::Left});
        REQUIRE(project->UpdateRuntime(client.steps[i]));
        const auto snapshot = project->GetRuntimeSession()->GetSnapshot();
        REQUIRE(snapshot);
        REQUIRE(snapshot->fields == snapshots[i].fields);
        REQUIRE(snapshot->frameIndex == snapshots[i].frameIndex);
        REQUIRE(snapshot->runtimeId == project->GetRuntimeStatus().runtimeId);
    }
    REQUIRE_FALSE(project->IsDirty());
}

TEST_CASE("Animation showcase Play Stop Switch matches Application and Editor frame for frame",
          "[animation][application]")
{
    RuntimeTestState state;
    state.frameEvents.resize(40);
    for (const auto [frame, key] :
         {std::pair{3, Janus::KeyCode::Space}, std::pair{5, Janus::KeyCode::Enter},
          std::pair{8, Janus::KeyCode::D}})
        state.frameEvents[frame] = {Janus::KeyPressedEvent{key, false},
                                    Janus::KeyReleasedEvent{key}};
    class Client final : public Janus::ApplicationClient
    {
      public:
        Janus::Result<void> OnInitialize(Janus::Application&) override
        {
            return Janus::Result<void>::Success();
        }
        void OnUpdate(Janus::TimeStep step, Janus::Application&) override
        {
            steps.push_back(step);
        }
        std::vector<Janus::TimeStep> steps;
    } client;
    const auto root = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "Game";
    Janus::ProjectRuntimeConfig projectConfig{root};
    projectConfig.startupScenePath = "Scenes/AnimationShowcase.scene";
    Janus::ApplicationConfig config;
    config.project = projectConfig;
    auto app = Janus::Detail::ApplicationTestAccess::Create(config, MakeDependencies(state));
    std::vector<Janus::ScriptSnapshot> snapshots;
    state.onPresent = [&]
    {
        REQUIRE(app->GetSnapshot());
        snapshots.push_back(*app->GetSnapshot());
        if (snapshots.size() == state.frameEvents.size())
            app->RequestExit();
    };
    auto result = app->Run(client);
    INFO((result ? "success" : result.GetError().message));
    REQUIRE(result);
    REQUIRE(snapshots.size() == 40);
    CHECK(std::get<bool>(snapshots[0].fields.at("playing")));
    CHECK_FALSE(std::get<bool>(snapshots[3].fields.at("playing")));
    CHECK(std::get<bool>(snapshots[5].fields.at("playing")));
    CHECK(std::get<double>(snapshots[8].fields.at("animationFrame")) == 0);
    CHECK_FALSE(std::get<bool>(snapshots.back().fields.at("playing")));
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open(projectConfig, *renderer);
    REQUIRE(opened);
    auto project = std::move(opened).Value();
    Janus::InputState input;
    REQUIRE(project->StartRuntime(input));
    for (Janus::usize i = 0; i < snapshots.size(); ++i)
    {
        input.BeginFrame();
        for (const auto& event : state.frameEvents[i])
            input.Apply(event);
        REQUIRE(project->UpdateRuntime(client.steps[i]));
        REQUIRE(project->GetRuntimeSession()->GetSnapshot()->fields == snapshots[i].fields);
    }
    REQUIRE(project->PauseRuntime());
    const auto id = Janus::UUID::Parse("ad100000-0000-4000-8000-000000000011").Value();
    const auto time = project->GetRuntimeSession()->GetAnimations().GetPose(id)->elapsedSeconds;
    REQUIRE(project->UpdateRuntime(Janus::TimeStep::FromSeconds(10)));
    CHECK(project->GetRuntimeSession()->GetAnimations().GetPose(id)->elapsedSeconds == time);
    REQUIRE(project->StopRuntime());
    CHECK_FALSE(project->IsDirty());
}

TEST_CASE("Physics showcase matches Application and Editor snapshots frame for frame",
          "[physics][application]")
{
    RuntimeTestState state;
    state.frameEvents.resize(180);
    class Client final : public Janus::ApplicationClient
    {
      public:
        Janus::Result<void> OnInitialize(Janus::Application&) override
        {
            return Janus::Result<void>::Success();
        }
        void OnUpdate(Janus::TimeStep step, Janus::Application&) override
        {
            steps.push_back(step);
        }
        std::vector<Janus::TimeStep> steps;
    } client;
    const auto root = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "Game";
    Janus::ProjectRuntimeConfig projectConfig{root};
    projectConfig.startupScenePath = "Scenes/PhysicsShowcase.scene";
    Janus::ApplicationConfig config;
    config.project = projectConfig;
    auto app = Janus::Detail::ApplicationTestAccess::Create(config, MakeDependencies(state));
    std::vector<Janus::ScriptSnapshot> snapshots;
    state.onPresent = [&]
    {
        REQUIRE(app->GetSnapshot());
        snapshots.push_back(*app->GetSnapshot());
        if (snapshots.size() == state.frameEvents.size())
            app->RequestExit();
    };
    auto result = app->Run(client);
    INFO((result ? "success" : result.GetError().message));
    REQUIRE(result);
    REQUIRE(snapshots.size() == 180);
    const auto& last = snapshots.back().fields;
    CHECK(std::get<double>(last.at("collisions")) >= 1);
    CHECK(std::get<double>(last.at("triggerEnters")) == 1);
    CHECK(std::get<double>(last.at("triggerExits")) == 1);
    CHECK(std::get<double>(last.at("bodies")) == 5);
    CHECK(std::get<bool>(last.at("rayFloor")));
    CHECK(std::get<double>(last.at("y")) == Catch::Approx(0).margin(0.03));
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open(projectConfig, *renderer);
    REQUIRE(opened);
    auto project = std::move(opened).Value();
    Janus::InputState input;
    REQUIRE(project->StartRuntime(input));
    for (Janus::usize i = 0; i < snapshots.size(); ++i)
    {
        REQUIRE(project->UpdateRuntime(client.steps[i]));
        REQUIRE(project->GetRuntimeSession()->GetSnapshot()->fields == snapshots[i].fields);
    }
    REQUIRE(project->StopRuntime());
    CHECK_FALSE(project->IsDirty());
}
