#include "Application/Application.h"
#include "Application/ApplicationClient.h"
#include "Application/ApplicationConfig.h"
#include "Application/Detail/ApplicationDependencies.h"

#include "Core/Event/Event.h"
#include "Core/Time/FrameClock.h"
#include "Platform/Graphics/GraphicsContext.h"
#include "Platform/Window/Window.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace
{

struct TestState
{
    std::vector<std::string> order;
    bool failPlatformInitialize = false;
    bool failWindowCreate = false;
    bool failContextCreate = false;
    bool failMakeCurrent = false;
    bool failSwapInterval = false;
    Janus::FrameClock::TimePoint now{};
    Janus::Test::FakeRenderDevice rendererDevice;
};

class FakeWindow final : public Janus::Window
{
public:
    explicit FakeWindow(TestState& state)
        : m_State(state)
    {
    }

    ~FakeWindow() override
    {
        m_State.order.emplace_back("window.destroy");
    }

    void PollEvents(const EventCallback& callback) override
    {
        m_State.order.emplace_back("window.poll");
        callback(Janus::WindowResizeEvent{800, 600});
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
        return m_ShouldClose;
    }

    void RequestClose() noexcept override
    {
        m_ShouldClose = true;
    }

    void* GetNativeHandle() const noexcept override
    {
        return nullptr;
    }

private:
    TestState& m_State;
    bool m_ShouldClose = false;
};

class FakeGraphicsContext final : public Janus::GraphicsContext
{
public:
    explicit FakeGraphicsContext(TestState& state)
        : m_State(state)
    {
    }

    ~FakeGraphicsContext() override
    {
        m_State.order.emplace_back("context.destroy");
    }

    Janus::Result<void> MakeCurrent() override
    {
        m_State.order.emplace_back("context.make_current");

        if (m_State.failMakeCurrent)
        {
            return Janus::Result<void>::Failure(
                Janus::ErrorCode::GraphicsContextMakeCurrentFailed,
                "context activation failed");
        }

        return Janus::Result<void>::Success();
    }

    Janus::Result<void> SetSwapInterval(Janus::i32) override
    {
        m_State.order.emplace_back("context.swap_interval");

        if (m_State.failSwapInterval)
        {
            return Janus::Result<void>::Failure(
                Janus::ErrorCode::SwapIntervalFailed,
                "swap interval unavailable");
        }

        return Janus::Result<void>::Success();
    }

    void Present() noexcept override
    {
        m_State.order.emplace_back("context.present");
    }

private:
    TestState& m_State;
};

class RecordingClient final : public Janus::ApplicationClient
{
public:
    RecordingClient(TestState& state, bool failInitialization = false)
        : m_State(state),
        m_FailInitialization(failInitialization)
    {
    }

    Janus::Result<void> OnInitialize(Janus::Application&) override
    {
        m_State.order.emplace_back("client.initialize");

        if (m_FailInitialization)
        {
            return Janus::Result<void>::Failure(
                Janus::ErrorCode::InvalidArgument,
                "client initialization failed");
        }

        return Janus::Result<void>::Success();
    }

    void OnEvent(const Janus::Event& event, Janus::Application&) override
    {
        if (std::holds_alternative<Janus::WindowResizeEvent>(event))
        {
            m_State.order.emplace_back("client.event.resize");
        }
    }

    void OnUpdate(Janus::TimeStep, Janus::Application& application) override
    {
        m_State.order.emplace_back("client.update");
        application.RequestExit();
    }

    void OnShutdown(Janus::Application&) noexcept override
    {
        m_State.order.emplace_back("client.shutdown");
    }

private:
    TestState& m_State;
    bool m_FailInitialization = false;
};

Janus::Detail::ApplicationDependencies MakeDependencies(TestState& state)
{
    Janus::Detail::ApplicationDependencies dependencies;

    dependencies.initializePlatform = [&state]
    {
        state.order.emplace_back("platform.initialize");

        if (state.failPlatformInitialize)
        {
            return Janus::Result<void>::Failure(
                Janus::ErrorCode::PlatformInitFailed,
                "platform initialization failed");
        }

        return Janus::Result<void>::Success();
    };

    dependencies.shutdownPlatform = [&state]
    {
        state.order.emplace_back("platform.shutdown");
    };

    dependencies.createWindow = [&state](const Janus::WindowConfig&)
    {
        state.order.emplace_back("window.create");

        if (state.failWindowCreate)
        {
            return Janus::Result<std::unique_ptr<Janus::Window>>::Failure(
                Janus::ErrorCode::WindowCreateFailed,
                "window creation failed");
        }

        std::unique_ptr<Janus::Window> window =
            std::make_unique<FakeWindow>(state);

        return Janus::Result<std::unique_ptr<Janus::Window>>::Success(
            std::move(window));
    };

    dependencies.createGraphicsContext = [&state](Janus::Window&)
    {
        state.order.emplace_back("context.create");

        if (state.failContextCreate)
        {
            return Janus::Result<
                std::unique_ptr<Janus::GraphicsContext>>::Failure(
                Janus::ErrorCode::GraphicsContextCreateFailed,
                "context creation failed");
        }

        std::unique_ptr<Janus::GraphicsContext> context =
            std::make_unique<FakeGraphicsContext>(state);

        return Janus::Result<
            std::unique_ptr<Janus::GraphicsContext>>::Success(
            std::move(context));
    };

    dependencies.createRenderer2D = [&state]
    {
        state.order.emplace_back("renderer.create");
        return Janus::Result<std::unique_ptr<Janus::Renderer2D>>::Success(
            Janus::Detail::Renderer2DTestAccess::Create(
                state.rendererDevice));
    };

    dependencies.createScene = [&state]
    {
        state.order.emplace_back("scene.create");
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

TEST_CASE("Application runs and cleans up in lifecycle order", "[application]")
{
    TestState state;
    RecordingClient client(state);
    auto application = Janus::Detail::ApplicationTestAccess::Create(
        Janus::ApplicationConfig{},
        MakeDependencies(state));

    REQUIRE(application->Run(client));
    REQUIRE(state.order == std::vector<std::string>{
        "platform.initialize",
        "window.create",
        "context.create",
        "context.make_current",
        "context.swap_interval",
        "renderer.create",
        "scene.create",
        "client.initialize",
        "window.poll",
        "client.event.resize",
        "client.update",
        "context.present",
        "client.shutdown",
        "context.destroy",
        "window.destroy",
        "platform.shutdown"});
}

TEST_CASE("Application cleans initialized resources when client initialization fails",
          "[application]")
{
    TestState state;
    RecordingClient client(state, true);
    auto application = Janus::Detail::ApplicationTestAccess::Create(
        Janus::ApplicationConfig{},
        MakeDependencies(state));

    const auto result = application->Run(client);

    REQUIRE_FALSE(result);
    REQUIRE(result.GetError().code == Janus::ErrorCode::InvalidArgument);
    REQUIRE(state.order == std::vector<std::string>{
        "platform.initialize",
        "window.create",
        "context.create",
        "context.make_current",
        "context.swap_interval",
        "renderer.create",
        "scene.create",
        "client.initialize",
        "context.destroy",
        "window.destroy",
        "platform.shutdown"});
}

TEST_CASE("Application cleans only resources completed before a backend failure",
          "[application]")
{
    TestState state;
    RecordingClient client(state);

    SECTION("platform initialization")
    {
        state.failPlatformInitialize = true;
        auto application = Janus::Detail::ApplicationTestAccess::Create(
            Janus::ApplicationConfig{},
            MakeDependencies(state));

        REQUIRE_FALSE(application->Run(client));
        REQUIRE(state.order == std::vector<std::string>{
            "platform.initialize"});
    }

    SECTION("window creation")
    {
        state.failWindowCreate = true;
        auto application = Janus::Detail::ApplicationTestAccess::Create(
            Janus::ApplicationConfig{},
            MakeDependencies(state));

        REQUIRE_FALSE(application->Run(client));
        REQUIRE(state.order == std::vector<std::string>{
            "platform.initialize",
            "window.create",
            "platform.shutdown"});
    }

    SECTION("context creation")
    {
        state.failContextCreate = true;
        auto application = Janus::Detail::ApplicationTestAccess::Create(
            Janus::ApplicationConfig{},
            MakeDependencies(state));

        REQUIRE_FALSE(application->Run(client));
        REQUIRE(state.order == std::vector<std::string>{
            "platform.initialize",
            "window.create",
            "context.create",
            "window.destroy",
            "platform.shutdown"});
    }

    SECTION("make current")
    {
        state.failMakeCurrent = true;
        auto application = Janus::Detail::ApplicationTestAccess::Create(
            Janus::ApplicationConfig{},
            MakeDependencies(state));

        REQUIRE_FALSE(application->Run(client));
        REQUIRE(state.order == std::vector<std::string>{
            "platform.initialize",
            "window.create",
            "context.create",
            "context.make_current",
            "context.destroy",
            "window.destroy",
            "platform.shutdown"});
    }
}

TEST_CASE("Application continues when VSync cannot be enabled", "[application]")
{
    TestState state;
    state.failSwapInterval = true;
    RecordingClient client(state);
    auto application = Janus::Detail::ApplicationTestAccess::Create(
        Janus::ApplicationConfig{},
        MakeDependencies(state));

    REQUIRE(application->Run(client));
    REQUIRE(std::ranges::count(state.order, "client.update") == 1);
    REQUIRE(std::ranges::count(state.order, "context.present") == 1);
}

TEST_CASE("Application rejects a second run", "[application]")
{
    TestState state;
    RecordingClient client(state);
    auto application = Janus::Detail::ApplicationTestAccess::Create(
        Janus::ApplicationConfig{},
        MakeDependencies(state));

    REQUIRE(application->Run(client));

    const auto secondRun = application->Run(client);
    REQUIRE_FALSE(secondRun);
    REQUIRE(secondRun.GetError().code == Janus::ErrorCode::InvalidState);
}


TEST_CASE(
    "Application client-driven mode leaves project runtime ownership to the client",
    "[application][v0.6][client-driven]")
{
    TestState state;
    RecordingClient client(state);

    Janus::ApplicationConfig config;
    config.executionMode =
        Janus::ApplicationExecutionMode::ClientDriven;

    auto application = Janus::Detail::ApplicationTestAccess::Create(
        config,
        MakeDependencies(state));

    REQUIRE(application->Run(client));
    REQUIRE(state.order == std::vector<std::string>{
        "platform.initialize",
        "window.create",
        "context.create",
        "context.make_current",
        "context.swap_interval",
        "renderer.create",
        "client.initialize",
        "window.poll",
        "client.event.resize",
        "client.update",
        "context.present",
        "client.shutdown",
        "context.destroy",
        "window.destroy",
        "platform.shutdown"});
    REQUIRE(
        std::ranges::count(state.order, "scene.create")
        == 0);
}
