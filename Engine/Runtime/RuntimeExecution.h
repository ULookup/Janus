#pragma once

#include "Animation/AnimationSystem.h"
#include "Audio/AudioSystem.h"
#include "Core/Error/Result.h"
#include "Core/Input/InputActions.h"
#include "Core/Input/InputState.h"
#include "Core/Time/TimeStep.h"
#include "Diagnostics/ScriptSnapshot.h"
#include "Physics/PhysicsSystem.h"
#include "UI/UIInteraction.h"

#include <memory>
#include <optional>

namespace Janus
{
class AssetService;
class Scene;
class ScriptEngine;
class CpuProfiler;

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
           const InputBindings& bindings = {}, Viewport logicalViewport = {1280, 720},
           AudioDeviceFactory audioFactory = {}, CpuProfiler* profiler = nullptr);
    ~RuntimeExecution();

    RuntimeExecution(const RuntimeExecution&) = delete;
    RuntimeExecution& operator=(const RuntimeExecution&) = delete;
    RuntimeExecution(RuntimeExecution&&) = delete;
    RuntimeExecution& operator=(RuntimeExecution&&) = delete;

    [[nodiscard]] Result<void> Start(bool audioSuspended = false);
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
    [[nodiscard]] std::optional<ScriptSnapshot> GetSnapshot() const;
    [[nodiscard]] UUID GetRuntimeId() const noexcept
    {
        return m_RuntimeId;
    }
    void SetAudioSuspended(bool suspended) noexcept
    {
        m_Audio->SetSuspended(suspended);
    }
    [[nodiscard]] const PhysicsSystem& GetPhysics() const noexcept
    {
        return *m_Physics;
    }
    [[nodiscard]] const AudioSystem& GetAudio() const noexcept
    {
        return *m_Audio;
    }
    [[nodiscard]] const AnimationSystem& GetAnimations() const noexcept
    {
        return *m_Animations;
    }

  private:
    explicit RuntimeExecution(Scene& scene, const InputState& initialInput,
                              Viewport logicalViewport);
    Scene& m_Scene;
    // Optional owner-thread host recorder; like Scene, it must outlive this execution.
    CpuProfiler* m_Profiler = nullptr;
    Viewport m_LogicalViewport;
    UIInteraction m_UI;

    // Lua borrows this stable buffer even when the caller supplies a temporary neutral frame.
    InputState m_Input;
    // Scripts borrow animations during callbacks, so they must be destroyed first.
    std::unique_ptr<AnimationSystem> m_Animations;
    std::unique_ptr<AudioSystem> m_Audio;
    std::unique_ptr<PhysicsSystem> m_Physics;
    std::unique_ptr<ScriptEngine> m_ScriptEngine;
    UUID m_RuntimeId;
    u64 m_FrameIndex = 0;
};
} // namespace Janus
