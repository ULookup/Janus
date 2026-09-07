#pragma once

#include "Core/Error/Result.h"
#include "Core/Input/InputState.h"
#include "Core/Log/LogStore.h"
#include "Core/Profiling/CpuProfiler.h"
#include "Core/Time/FrameClock.h"
#include "Diagnostics/ScriptSnapshot.h"

#include "Application/ApplicationConfig.h"
#include "Application/Detail/ApplicationDependencies.h"

#include <memory>
#include <optional>

namespace Janus
{

class AssetRegistry;
class AssetService;
class Window;
class GraphicsContext;
class ReflectionRegistry;
class Renderer2D;
class Scene;
class SceneRenderer;
class RuntimeExecution;
class ApplicationClient;

class Application final
{
public:
    explicit Application(ApplicationConfig config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] Result<void> Run(ApplicationClient& client);
    void RequestExit() noexcept;
    [[nodiscard]] std::shared_ptr<LogStore> GetLogStore() const noexcept
    {
        return m_Logs;
    }
    [[nodiscard]] const InputState& GetInput() const noexcept;
    [[nodiscard]] Window& GetWindow() noexcept;
    [[nodiscard]] Renderer2D& GetRenderer2D() noexcept;
    [[nodiscard]] ReflectionRegistry& GetReflectionRegistry() noexcept;
    [[nodiscard]] const ReflectionRegistry& GetReflectionRegistry() const noexcept;
    [[nodiscard]] Scene& GetScene() noexcept;
    [[nodiscard]] std::optional<ScriptSnapshot> GetSnapshot() const;
    [[nodiscard]] const CpuProfiler& GetProfiler() const noexcept
    {
        return m_Profiler;
    }

  private:
    Application(
        ApplicationConfig config,
        Detail::ApplicationDependencies dependencies);

    void Cleanup(ApplicationClient* client, bool callClientShutdown);

    ApplicationConfig m_Config;
    std::shared_ptr<LogStore> m_Logs = std::make_shared<LogStore>();
    Detail::ApplicationDependencies m_Dependencies;
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<GraphicsContext> m_GraphicsContext;
    std::unique_ptr<Renderer2D> m_Renderer2D;
    std::unique_ptr<ReflectionRegistry> m_ReflectionRegistry;
    std::unique_ptr<AssetRegistry> m_AssetRegistry;
    std::unique_ptr<AssetService> m_AssetService;
    std::unique_ptr<SceneRenderer> m_SceneRenderer;
    std::unique_ptr<Scene> m_Scene;
    CpuProfiler m_Profiler;
    std::unique_ptr<RuntimeExecution> m_Execution;
    InputState m_Input;
    FrameClock m_FrameClock;

    bool m_PlatformInitialized = false;
    bool m_ClientInitialized = false;
    bool m_ExitRequested = false;
    bool m_HasRun = false;

    friend struct Detail::ApplicationTestAccess;
};

} // namespace Janus
