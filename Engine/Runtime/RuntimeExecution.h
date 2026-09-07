#pragma once

#include "Core/Error/Result.h"
#include "Core/Input/InputActions.h"
#include "Core/Input/InputState.h"
#include "Core/Time/TimeStep.h"
#include "UI/UIInteraction.h"

#include <memory>

namespace Janus
{
class AssetService;
class Scene;
class ScriptEngine;

enum class ScriptReloadPolicy
{
    CheckForChanges,
    Skip
};

// The host owns Scene and AssetService; both must outlive this execution object.
// Host state machines and rendering stay outside the shared simulation stages.
class RuntimeExecution final
{
  public:
    [[nodiscard]] static Result<std::unique_ptr<RuntimeExecution>>
    Create(Scene& scene, AssetService& assets, const InputState& initialInput,
           const InputBindings& bindings = {}, Viewport logicalViewport = {1280, 720});
    ~RuntimeExecution();

    RuntimeExecution(const RuntimeExecution&) = delete;
    RuntimeExecution& operator=(const RuntimeExecution&) = delete;
    RuntimeExecution(RuntimeExecution&&) = delete;
    RuntimeExecution& operator=(RuntimeExecution&&) = delete;

    [[nodiscard]] Result<void> Start();
    [[nodiscard]] Result<void>
    Advance(TimeStep timeStep, const InputState& input,
            ScriptReloadPolicy reload = ScriptReloadPolicy::CheckForChanges,
            bool dispatchUI = true);
    void PrimeUI(const InputState& input)
    {
        m_UI.Cancel();
        m_UI.Prime(input);
    }
    void CancelUI() noexcept
    {
        m_UI.Cancel();
    }
    [[nodiscard]] const UIInteractionState& GetUIState() const noexcept
    {
        return m_UI.GetState();
    }
    [[nodiscard]] Result<void> Stop();
    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] usize InstanceCount() const noexcept;

  private:
    explicit RuntimeExecution(Scene& scene, const InputState& initialInput,
                              Viewport logicalViewport);
    Scene& m_Scene;
    Viewport m_LogicalViewport;
    UIInteraction m_UI;

    // Lua borrows this stable buffer even when the caller supplies a temporary neutral frame.
    InputState m_Input;
    std::unique_ptr<ScriptEngine> m_ScriptEngine;
};
} // namespace Janus
