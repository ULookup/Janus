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
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneRenderer.h"
#include "Scene/SceneReflection.h"
#include "Scripting/ScriptEngine.h"

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

    return dependencies;
}

} // namespace Janus::Detail

namespace Janus
{
namespace
{

Result<std::filesystem::path> ResolveProjectFile(
    const ProjectRuntimeConfig& project,
    const std::filesystem::path& relativePath,
    std::string_view label)
{
    if (project.root.empty())
    {
        return Result<std::filesystem::path>::Failure(
            ErrorCode::InvalidArgument,
            "Project root must not be empty.");
    }

    if (relativePath.empty()
        || relativePath.is_absolute()
        || relativePath.has_root_name()
        || relativePath.has_root_directory())
    {
        return Result<std::filesystem::path>::Failure(
            ErrorCode::InvalidArgument,
            std::string(label) + " must be project-relative.");
    }

    const std::filesystem::path normalized = relativePath.lexically_normal();
    for (const auto& part : normalized)
    {
        if (part == std::filesystem::path(".."))
        {
            return Result<std::filesystem::path>::Failure(
                ErrorCode::InvalidArgument,
                std::string(label) + " cannot escape the project root.");
        }
    }

    return Result<std::filesystem::path>::Success(
        project.root / normalized);
}

} // namespace

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

            auto registryPath = ResolveProjectFile(
                project,
                project.assetRegistryPath,
                "Asset registry path");
            if (!registryPath)
            {
                Error error = std::move(registryPath.GetError());
                Cleanup(&client, false);
                return Result<void>::Failure(std::move(error));
            }

            auto scenePath = ResolveProjectFile(
                project,
                project.startupScenePath,
                "Startup Scene path");
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
        auto scriptEngineResult = ScriptEngine::Create(
            *m_Scene,
            *m_AssetService,
            m_Input);
        if (!scriptEngineResult)
        {
            Error error = std::move(scriptEngineResult.GetError());
            Cleanup(&client, true);
            return Result<void>::Failure(std::move(error));
        }

        m_ScriptEngine = std::move(scriptEngineResult).Value();

        auto scriptStartResult = m_ScriptEngine->Start();
        if (!scriptStartResult)
        {
            Error error = std::move(scriptStartResult.GetError());
            Cleanup(&client, true);
            return Result<void>::Failure(std::move(error));
        }
    }

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

        if (managedRuntime)
        {
            const auto reloadResult = m_ScriptEngine->ReloadChangedScripts();
            if (!reloadResult)
            {
                Error error = reloadResult.GetError();
                JANUS_CORE_ERROR("Lua hot reload failed: {}", error.message);

                if (diskBackedProject)
                {
                    Cleanup(&client, true);
                    return Result<void>::Failure(std::move(error));
                }

                RequestExit();
            }
            else
            {
                const auto scriptUpdateResult =
                    m_ScriptEngine->Update(timeStep);
                if (!scriptUpdateResult)
                {
                    Error error = scriptUpdateResult.GetError();
                    JANUS_CORE_ERROR("Lua update failed: {}", error.message);

                    if (diskBackedProject)
                    {
                        Cleanup(&client, true);
                        return Result<void>::Failure(std::move(error));
                    }

                    RequestExit();
                }
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
    // ScriptEngine must stop and release its VM before Scene teardown.
    if (m_ScriptEngine != nullptr)
    {
        const auto stopped = m_ScriptEngine->Stop();
        if (!stopped)
        {
            JANUS_CORE_ERROR(
                "Lua shutdown failed: {}",
                stopped.GetError().message);
        }
        m_ScriptEngine.reset();
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
