#include "Application/Application.h"

#include "Application/ApplicationClient.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Core/Assert.h"
#include "Core/Event/Event.h"
#include "Core/Log/Log.h"
#include "Platform/Graphics/GraphicsContext.h"
#include "Platform/Platform.h"
#include "Platform/Window/Window.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderer.h"

#include <filesystem>
#include <memory>
#include <utility>
#include <variant>

namespace Janus::Detail
{

ApplicationDependencies CreateDefaultApplicationDependencies()
{
    ApplicationDependencies dependencies;

    dependencies.initializePlatform = []
    {
        return Platform::Initialize();
    };

    dependencies.shutdownPlatform = []
    {
        Platform::Shutdown();
    };

    dependencies.createWindow = [](const WindowConfig& config)
    {
        return Window::Create(config);
    };

    dependencies.createGraphicsContext = [](Window& window)
    {
        return GraphicsContext::Create(window);
    };

    dependencies.createRenderer2D = []
    {
        return Renderer2D::Create();
    };

    dependencies.createScene = []
    {
        return std::make_unique<Scene>();
    };

    dependencies.now = []
    {
        return FrameClock::Clock::now();
    };

    return dependencies;
}

} // namespace Janus::Detail

namespace Janus
{

Application::Application(ApplicationConfig config)
    : Application(
        std::move(config),
        Detail::CreateDefaultApplicationDependencies())
{
}

Application::Application(
    ApplicationConfig config,
    Detail::ApplicationDependencies dependencies)
    : m_Config(std::move(config)),
      m_Dependencies(std::move(dependencies))
{
}

Application::~Application()
{
    Cleanup(nullptr, false);
}

Result<void> Application::Run(ApplicationClient& client)
{
    if (m_HasRun)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Application::Run may only be called once.");
    }

    m_HasRun = true;

    Log::Initialize();

    auto platformResult = m_Dependencies.initializePlatform();
    if (!platformResult)
    {
        Error error = std::move(platformResult.GetError());
        Cleanup(&client, false);
        return Result<void>::Failure(std::move(error));
    }

    m_PlatformInitialized = true;

    auto windowResult = m_Dependencies.createWindow(m_Config.window);
    if (!windowResult)
    {
        Error error = std::move(windowResult.GetError());
        Cleanup(&client, false);
        return Result<void>::Failure(std::move(error));
    }

    m_Window = std::move(windowResult).Value();

    auto contextResult = m_Dependencies.createGraphicsContext(*m_Window);
    if (!contextResult)
    {
        Error error = std::move(contextResult.GetError());
        Cleanup(&client, false);
        return Result<void>::Failure(std::move(error));
    }

    m_GraphicsContext = std::move(contextResult).Value();

    auto makeCurrentResult = m_GraphicsContext->MakeCurrent();
    if (!makeCurrentResult)
    {
        Error error = std::move(makeCurrentResult.GetError());
        Cleanup(&client, false);
        return Result<void>::Failure(std::move(error));
    }

    const auto swapIntervalResult =
        m_GraphicsContext->SetSwapInterval(1);

    if (!swapIntervalResult)
    {
        JANUS_CORE_WARN(
            "Failed to enable VSync; continuing: {}",
            swapIntervalResult.GetError().message);
    }

    auto rendererResult = m_Dependencies.createRenderer2D();
    if (!rendererResult)
    {
        Error error = std::move(rendererResult.GetError());
        Cleanup(&client, false);
        return Result<void>::Failure(std::move(error));
    }

    m_Renderer2D = std::move(rendererResult).Value();

    // #12 replaces this empty runtime registry/current-directory root with
    // the disk-backed project configuration. #11 only establishes the
    // ownership and render dependency chain required by SceneRenderer.
    m_AssetRegistry = std::make_unique<AssetRegistry>();
    m_AssetService = std::make_unique<AssetService>(
        std::filesystem::current_path(),
        *m_AssetRegistry,
        *m_Renderer2D);
    m_SceneRenderer = std::make_unique<SceneRenderer>();
    m_Scene = m_Dependencies.createScene();

    auto initializeResult = client.OnInitialize(*this);
    if (!initializeResult)
    {
        Error error = std::move(initializeResult.GetError());
        Cleanup(&client, false);
        return Result<void>::Failure(std::move(error));
    }

    m_ClientInitialized = true;

    while (!m_ExitRequested && !m_Window->ShouldClose())
    {
        m_Input.BeginFrame();

        m_Window->PollEvents(
            [this, &client](const Event& event)
            {
                m_Input.Apply(event);

                if (std::holds_alternative<WindowCloseEvent>(event))
                {
                    RequestExit();
                }

                client.OnEvent(event, *this);
            });

        const auto timeStep =
            m_FrameClock.Tick(m_Dependencies.now())
                .ClampedTo(m_Config.maximumFrameTime);

        client.OnUpdate(timeStep, *this);

        const Viewport viewport{
            m_Window->GetWidth(),
            m_Window->GetHeight()};

        const auto renderResult = m_SceneRenderer->Render(
            *m_Scene,
            *m_AssetService,
            *m_Renderer2D,
            viewport);

        if (!renderResult)
        {
            JANUS_CORE_ERROR(
                "Scene render failed: {}",
                renderResult.GetError().message);
            RequestExit();
        }

        m_GraphicsContext->Present();
    }

    Cleanup(&client, true);

    return Result<void>::Success();
}

void Application::RequestExit() noexcept
{
    m_ExitRequested = true;
}

const InputState& Application::GetInput() const noexcept
{
    return m_Input;
}

Renderer2D& Application::GetRenderer2D() noexcept
{
    JANUS_CORE_ASSERT(
        m_Renderer2D != nullptr,
        "Renderer2D is not available before Application::Run.");
    return *m_Renderer2D;
}

Scene& Application::GetScene() noexcept
{
    JANUS_CORE_ASSERT(
        m_Scene != nullptr,
        "Scene is not available before Application::Run.");
    return *m_Scene;
}

void Application::Cleanup(
    ApplicationClient* client,
    bool callClientShutdown)
{
    if (callClientShutdown && client != nullptr && m_ClientInitialized)
    {
        client->OnShutdown(*this);
        m_ClientInitialized = false;
    }

    // AssetService owns runtime GPU resources, so it must be destroyed before
    // Renderer2D and the graphics context. SceneRenderer is stateless but is
    // reset before its dependencies for an explicit lifecycle order.
    m_Scene.reset();
    m_SceneRenderer.reset();
    m_AssetService.reset();
    m_AssetRegistry.reset();
    m_Renderer2D.reset();
    m_GraphicsContext.reset();
    m_Window.reset();

    if (m_PlatformInitialized)
    {
        m_Dependencies.shutdownPlatform();
        m_PlatformInitialized = false;
    }

    Log::Shutdown();
}

} // namespace Janus
