#include "Runtime/RuntimeExecution.h"

#include "Core/Log/Log.h"
#include "Core/Profiling/CpuProfiler.h"
#include "Scripting/ScriptEngine.h"

#include <cmath>
#include <utility>

namespace Janus
{
RuntimeExecution::RuntimeExecution(Scene& scene, const InputState& initialInput,
                                   Viewport logicalViewport)
    : m_Scene(scene), m_LogicalViewport(logicalViewport), m_Input(initialInput)
{
    m_UI.Prime(initialInput);
}

Result<std::unique_ptr<RuntimeExecution>>
RuntimeExecution::Create(Scene& scene, AssetService& assets, const InputState& initialInput,
                         const InputBindings& bindings, Viewport logicalViewport,
                         AudioDeviceFactory audioFactory, CpuProfiler* profiler)
{
    auto execution = std::unique_ptr<RuntimeExecution>(
        new RuntimeExecution(scene, initialInput, logicalViewport));
    execution->m_Profiler = profiler;
    execution->m_Animations = std::make_unique<AnimationSystem>(scene, assets);
    execution->m_Audio = std::make_unique<AudioSystem>(scene, assets, std::move(audioFactory));
    execution->m_Physics = std::make_unique<PhysicsSystem>(scene);
    auto scripts = ScriptEngine::Create(scene, assets, execution->m_Input, bindings);
    if (!scripts)
        return Result<std::unique_ptr<RuntimeExecution>>::Failure(scripts.GetError());
    execution->m_ScriptEngine = std::move(scripts).Value();
    execution->m_ScriptEngine->SetPhysics(execution->m_Physics.get());
    execution->m_ScriptEngine->SetAudio(execution->m_Audio.get());
    execution->m_ScriptEngine->SetAnimations(execution->m_Animations.get());
    return Result<std::unique_ptr<RuntimeExecution>>::Success(std::move(execution));
}

RuntimeExecution::~RuntimeExecution()
{
    // Run OnDestroy while the borrowed Scene/assets and owned input are still alive.
    auto stopped = Stop();
    if (!stopped)
        JANUS_CORE_ERROR("Runtime execution shutdown: {}", stopped.GetError().message);
}

Result<void> RuntimeExecution::Start(bool audioSuspended)
{
    if (IsRunning())
        return Result<void>::Failure(ErrorCode::InvalidState, "Runtime is already started.");
    m_RuntimeId = UUID::Random();
    m_FrameIndex = 0;
    m_ScriptEngine->SetSnapshotContext(m_RuntimeId, 0);
    auto animations = m_Animations->Start();
    if (!animations)
        return animations;
    m_Audio->SetSuspended(audioSuspended);
    auto audio = m_Audio->Start();
    if (!audio)
    {
        m_Animations->Stop();
        return audio;
    }
    auto physics = m_Physics->Start();
    if (!physics)
    {
        m_Audio->Stop();
        m_Animations->Stop();
        return physics;
    }
    auto scripts = m_ScriptEngine->Start();
    if (!scripts)
    {
        m_Physics->Stop();
        m_Audio->Stop();
        m_Animations->Stop();
    }
    return scripts;
}

Result<void> RuntimeExecution::Advance(TimeStep timeStep, const InputState& input,
                                       ScriptReloadPolicy reload, bool dispatchUI)
{
    if (!std::isfinite(timeStep.GetSeconds()))
        return Result<void>::Failure(ErrorCode::InvalidArgument, "Invalid runtime timestep.");
    if (!IsRunning())
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Runtime execution must be started before Advance.");
    // Any callback/content failure must silence already queued audio before host fault retention.
    struct AudioFaultGuard
    {
        AudioSystem& audio;
        bool succeeded = false;
        ~AudioFaultGuard()
        {
            if (!succeeded)
                audio.SetSuspended(true);
        }
    } audioGuard{*m_Audio};
    const auto profile = [this](std::string_view name, auto&& action)
    {
        std::optional<CpuScope> scope;
        if (m_Profiler)
            scope.emplace(*m_Profiler, name);
        return action();
    };
    std::vector<UUID> clicks;
    auto ui = profile("Runtime.UI",
                      [&]() -> Result<void>
                      {
                          if (dispatchUI)
                          {
                              auto layout =
                                  UILayout::Build(m_Scene, m_LogicalViewport, m_Animations.get());
                              if (!layout)
                                  return Result<void>::Failure(layout.GetError());
                              auto routed = m_UI.Process(m_Scene, layout.Value(), input);
                              m_Input = std::move(routed.gameplay);
                              clicks = std::move(routed.clicks);
                          }
                          else
                          {
                              m_UI.Cancel();
                              m_Input = input;
                          }
                          return Result<void>::Success();
                      });
    if (!ui)
        return ui;
    // Publications during reload, clicks and Update share the attempted simulation frame.
    m_ScriptEngine->SetSnapshotContext(m_RuntimeId, ++m_FrameIndex);
    if (reload == ScriptReloadPolicy::CheckForChanges)
    {
        auto reloaded =
            profile("Runtime.Reload", [&] { return m_ScriptEngine->ReloadChangedScripts(); });
        if (!reloaded)
            return reloaded;
    }
    auto updated = profile("Runtime.Lua",
                           [&]() -> Result<void>
                           {
                               for (const auto id : clicks)
                               {
                                   auto dispatched = m_ScriptEngine->DispatchButtonClick(id);
                                   if (!dispatched)
                                       return dispatched;
                               }
                               return m_ScriptEngine->Update(timeStep);
                           });
    if (!updated)
        return updated;
    auto physics =
        profile("Runtime.Physics",
                [&]
                {
                    return m_Physics->Advance(
                        timeStep, [this](const PhysicsEvent& event)
                        { return m_ScriptEngine->DispatchPhysicsEvent(event); }, [this](UUID entity)
                        { return m_ScriptEngine->DestroyPhysicsEntity(entity); });
                });
    if (!physics)
        return physics;
    auto animated = profile("Runtime.Animation", [&] { return m_Animations->Advance(timeStep); });
    if (!animated)
        return animated;
    auto audio = profile("Runtime.Audio", [&] { return m_Audio->Advance(timeStep); });
    audioGuard.succeeded = static_cast<bool>(audio);
    return audio;
}

Result<void> RuntimeExecution::Stop()
{
    m_UI.Cancel();
    if (m_Audio)
        m_Audio->SetSuspended(true);
    auto result = m_ScriptEngine ? m_ScriptEngine->Stop() : Result<void>::Success();
    if (m_Physics)
        m_Physics->Stop();
    if (m_Audio)
        m_Audio->Stop();
    if (m_Animations)
        m_Animations->Stop();
    return result;
}

bool RuntimeExecution::IsRunning() const noexcept
{
    return m_ScriptEngine && m_ScriptEngine->IsRunning();
}

usize RuntimeExecution::InstanceCount() const noexcept
{
    return m_ScriptEngine ? m_ScriptEngine->InstanceCount() : 0;
}
std::optional<ScriptSnapshot> RuntimeExecution::GetSnapshot() const
{
    return m_ScriptEngine ? m_ScriptEngine->GetSnapshot() : std::nullopt;
}
} // namespace Janus
