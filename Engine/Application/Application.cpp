#include "Application/Application.h"

#include "Application/ApplicationClient.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Core/Assert.h"
#include "Core/Event/Event.h"
#include "Core/Log/Log.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Platform/Graphics/GraphicsContext.h"
#include "Platform/Platform.h"
#include "Platform/Window/Window.h"
#include "Project/ProjectSettings.h"
#include "Renderer/Renderer2D.h"
#include "Runtime/RuntimeExecution.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneRenderer.h"
#include <thread>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
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

    dependencies.sleepUntil = [](FrameClock::TimePoint time)
    { std::this_thread::sleep_until(time); };
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

    Log::Initialize(m_Config.logOutput, m_Logs);

    ProjectSettings settings;
    if (m_Config.executionMode == ApplicationExecutionMode::ManagedRuntime && m_Config.project)
    {
        auto loaded = LoadProjectSettings(*m_Config.project);
        if (!loaded)
        {
            auto error = loaded.GetError();
            Cleanup(&client, false);
            return Result<void>::Failure(error);
        }
        settings = std::move(loaded).Value();
        m_Config.window.width = settings.width;
        m_Config.window.height = settings.height;
        m_Config.window.title = settings.name;
        m_Config.vsync = settings.vsync;
        m_Config.targetFps = settings.targetFps;
    }
    if (m_Config.targetFps > 1000)
    {
        Cleanup(&client, false);
        return Result<void>::Failure(ErrorCode::InvalidArgument, "Target FPS must be 0-1000.");
    }
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

    const auto swapIntervalResult = m_GraphicsContext->SetSwapInterval(m_Config.vsync ? 1 : 0);

    if (!swapIntervalResult)
    {
        JANUS_CORE_WARN("Failed to configure VSync; continuing: {}",
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

    const bool managedRuntime =
        m_Config.executionMode == ApplicationExecutionMode::ManagedRuntime;
    const bool diskBackedProject =
        managedRuntime && m_Config.project.has_value();

    if (managedRuntime)
    {
        auto reflectionResult =
            CreateBuiltinSceneReflectionRegistry();
        if (!reflectionResult)
        {
            Error error = std::move(reflectionResult.GetError());
            Cleanup(&client, false);
            return Result<void>::Failure(std::move(error));
        }

        m_ReflectionRegistry =
            std::make_unique<ReflectionRegistry>(
                std::move(reflectionResult).Value());

        std::filesystem::path projectRoot = std::filesystem::current_path();

        if (diskBackedProject)
        {
            const ProjectRuntimeConfig& project = *m_Config.project;
            projectRoot = project.root;

            auto registryPath = ResolveProjectPath(project.root, settings.assetRegistry);
            if (!registryPath)
            {
                Error error = std::move(registryPath.GetError());
                Cleanup(&client, false);
                return Result<void>::Failure(std::move(error));
            }

            auto scenePath = ResolveProjectPath(project.root, settings.defaultScene);
            if (!scenePath)
            {
                Error error = std::move(scenePath.GetError());
                Cleanup(&client, false);
                return Result<void>::Failure(std::move(error));
            }

            auto registryResult = AssetRegistry::Load(registryPath.Value());
            if (!registryResult)
            {
                Error error = std::move(registryResult.GetError());
                Cleanup(&client, false);
                return Result<void>::Failure(std::move(error));
            }

            auto sceneResult = SceneDeserializer::Load(
                scenePath.Value(),
                *m_ReflectionRegistry);
            if (!sceneResult)
            {
                Error error = std::move(sceneResult.GetError());
                Cleanup(&client, false);
                return Result<void>::Failure(std::move(error));
            }

            m_AssetRegistry = std::make_unique<AssetRegistry>(
                std::move(registryResult).Value());
            m_Scene = std::move(sceneResult).Value();
        }
        else
        {
            m_AssetRegistry = std::make_unique<AssetRegistry>();
            m_Scene = m_Dependencies.createScene();
        }

        m_AssetService = std::make_unique<AssetService>(
            projectRoot,
            *m_AssetRegistry,
            *m_Renderer2D);
        m_SceneRenderer = std::make_unique<SceneRenderer>();
    }

    auto initializeResult = client.OnInitialize(*this);
    if (!initializeResult)
    {
        Error error = std::move(initializeResult.GetError());
        Cleanup(&client, false);
        return Result<void>::Failure(std::move(error));
    }

    m_ClientInitialized = true;

    if (managedRuntime)
    {
        auto executionResult =
            RuntimeExecution::Create(*m_Scene, *m_AssetService, m_Input, settings.inputBindings);
        if (!executionResult)
        {
            Error error = std::move(executionResult.GetError());
            Cleanup(&client, true);
            return Result<void>::Failure(std::move(error));
        }

        m_Execution = std::move(executionResult).Value();

        auto scriptStartResult = m_Execution->Start();
        if (!scriptStartResult)
        {
            Error error = std::move(scriptStartResult.GetError());
            Cleanup(&client, true);
            return Result<void>::Failure(std::move(error));
        }
    }

    while (!m_ExitRequested && !m_Window->ShouldClose())
    {
        const auto frameStart =
            m_Config.targetFps != 0 ? m_Dependencies.now() : FrameClock::TimePoint{};
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

        if (managedRuntime)
        {
            const auto advanceResult = m_Execution->Advance(timeStep, m_Input);
            if (!advanceResult)
            {
                Error error = advanceResult.GetError();
                JANUS_CORE_ERROR("Runtime advance failed: {}", error.message);

                if (diskBackedProject)
                {
                    Cleanup(&client, true);
                    return Result<void>::Failure(std::move(error));
                }

                RequestExit();
            }

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
                Error error = renderResult.GetError();
                JANUS_CORE_ERROR("Scene render failed: {}", error.message);

                if (diskBackedProject)
                {
                    Cleanup(&client, true);
                    return Result<void>::Failure(std::move(error));
                }

                RequestExit();
            }
        }

        m_GraphicsContext->Present();
        if (m_Config.targetFps != 0 && m_Dependencies.sleepUntil)
            m_Dependencies.sleepUntil(frameStart +
                                      std::chrono::duration_cast<FrameClock::Clock::duration>(
                                          std::chrono::duration<double>(1.0 / m_Config.targetFps)));
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

Window& Application::GetWindow() noexcept
{
    JANUS_CORE_ASSERT(
        m_Window != nullptr,
        "Window is not available before Application::Run.");
    return *m_Window;
}

Renderer2D& Application::GetRenderer2D() noexcept
{
    JANUS_CORE_ASSERT(
        m_Renderer2D != nullptr,
        "Renderer2D is not available before Application::Run.");
    return *m_Renderer2D;
}

ReflectionRegistry& Application::GetReflectionRegistry() noexcept
{
    JANUS_CORE_ASSERT(
        m_ReflectionRegistry != nullptr,
        "ReflectionRegistry is not available before managed runtime initialization.");
    return *m_ReflectionRegistry;
}

const ReflectionRegistry& Application::GetReflectionRegistry() const noexcept
{
    JANUS_CORE_ASSERT(
        m_ReflectionRegistry != nullptr,
        "ReflectionRegistry is not available before managed runtime initialization.");
    return *m_ReflectionRegistry;
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

    // Script callbacks can resolve Scene entities during OnDestroy, so the
    // Runtime execution must stop and release its VM before Scene teardown.
    if (m_Execution != nullptr)
    {
        const auto stopped = m_Execution->Stop();
        if (!stopped)
        {
            JANUS_CORE_ERROR(
                "Lua shutdown failed: {}",
                stopped.GetError().message);
        }
        m_Execution.reset();
    }

    // AssetService owns runtime GPU resources, so it must be destroyed before
    // Renderer2D and the graphics context. SceneRenderer is stateless but is
    // reset before its dependencies for an explicit lifecycle order.
    m_Scene.reset();
    m_SceneRenderer.reset();
    m_ReflectionRegistry.reset();
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
