#pragma once
#include "Core/Error/Result.h"
#include "Core/Input/InputActions.h"
#include "Core/Input/InputState.h"
#include "Core/Time/TimeStep.h"
#include "Core/UUID/UUID.h"
#include <memory>
#include <optional>
#include <string_view>

namespace Janus
{
class AssetService;
class ReflectionRegistry;
class Scene;
class ScriptEngine;
enum class RuntimeState
{
    Stopped,
    Playing,
    Paused,
    Faulted
};
[[nodiscard]] std::string_view RuntimeStateName(RuntimeState state) noexcept;
struct RuntimeStatus
{
    RuntimeState state = RuntimeState::Stopped;
    UUID runtimeId;
    u64 frameIndex = 0;
    f64 simulationTimeSeconds = 0;
    f64 lastStepSeconds = 0;
    usize entityCount = 0;
    usize scriptInstanceCount = 0;
    std::optional<Error> lastError;
    u64 failedFrameIndex = 0;
    bool partialUpdate = false;
};
class RuntimeSession final
{
  public:
    [[nodiscard]] static Result<std::unique_ptr<RuntimeSession>>
    Start(const Scene& editorScene, const ReflectionRegistry& reflection, AssetService& assets,
          const InputState& input, bool startPaused = false, const InputBindings& bindings = {});
    ~RuntimeSession();
    RuntimeSession(const RuntimeSession&) = delete;
    RuntimeSession& operator=(const RuntimeSession&) = delete;
    [[nodiscard]] Result<void> Update(TimeStep timeStep);
    [[nodiscard]] Result<void> Pause();
    [[nodiscard]] Result<void> Resume();
    [[nodiscard]] Result<void> Step();
    [[nodiscard]] Result<void> Stop();
    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] RuntimeState GetState() const noexcept
    {
        return m_Status.state;
    }
    [[nodiscard]] RuntimeStatus GetStatus() const;
    [[nodiscard]] Scene& GetScene() noexcept;
    [[nodiscard]] const Scene& GetScene() const noexcept;

  private:
    RuntimeSession(std::unique_ptr<Scene> scene, const InputState& input);
    [[nodiscard]] Result<void> Advance(TimeStep timeStep, bool reload);
    std::unique_ptr<Scene> m_RuntimeScene;
    const InputState& m_SourceInput;
    // ScriptEngine observes a stable owned buffer, including during neutral-input steps.
    InputState m_Input;
    std::unique_ptr<ScriptEngine> m_ScriptEngine;
    RuntimeStatus m_Status;
};
} // namespace Janus
