#include "Application/Application.h"
#include "Application/ApplicationClient.h"
#include "Application/ApplicationConfig.h"
#include "Application/Detail/ApplicationDependencies.h"

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

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
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
    Janus::usize pollCount = 0;
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
    Janus::Result<void> MakeCurrent() override
    {
        return Janus::Result<void>::Success();
    }

    Janus::Result<void> SetSwapInterval(Janus::i32) override
    {
        return Janus::Result<void>::Success();
    }

    void Present() noexcept override
    {
    }
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
                Janus::SceneSerializer::Save(scene, *m_RoundTripPath);
            if (!saveResult)
            {
                return Janus::Result<void>::Failure(saveResult.GetError());
            }

            auto loaded = Janus::SceneDeserializer::Load(*m_RoundTripPath);
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

    dependencies.createGraphicsContext = [](Janus::Window&)
    {
        std::unique_ptr<Janus::GraphicsContext> context =
            std::make_unique<FakeGraphicsContext>();
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
