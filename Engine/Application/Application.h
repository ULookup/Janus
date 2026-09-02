#pragma once

#include "Core/Error/Result.h"
#include "Core/Input/InputState.h"
#include "Core/Time/FrameClock.h"

#include "Application/ApplicationConfig.h"
#include "Application/Detail/ApplicationDependencies.h"

#include <memory>

namespace Janus
{

class Window;
class GraphicsContext;
class Renderer2D;
class Scene;
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
    [[nodiscard]] const InputState& GetInput() const noexcept;
    [[nodiscard]] Renderer2D& GetRenderer2D() noexcept;
    [[nodiscard]] Scene& GetScene() noexcept;

private:
    Application(
        ApplicationConfig config,
        Detail::ApplicationDependencies dependencies);

    void Cleanup(ApplicationClient* client, bool callClientShutdown);

    ApplicationConfig m_Config;
    Detail::ApplicationDependencies m_Dependencies;
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<GraphicsContext> m_GraphicsContext;
    std::unique_ptr<Renderer2D> m_Renderer2D;
    std::unique_ptr<Scene> m_Scene;
    InputState m_Input;
    FrameClock m_FrameClock;

    bool m_PlatformInitialized = false;
    bool m_ClientInitialized = false;
    bool m_ExitRequested = false;
    bool m_HasRun = false;

    friend struct Detail::ApplicationTestAccess;
};

} // namespace Janus
