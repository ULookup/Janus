#include "Runtime/RuntimeSession.h"
#include "Asset/AssetService.h"
#include "Core/Log/Log.h"
#include "Runtime/RuntimeExecution.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneCloner.h"
#include <cmath>
#include <utility>

namespace Janus
{
namespace
{
Error BoundedRuntimeError(Error error)
{
    usize limit = 8192;
    if (error.message.size() > limit)
    {
        while (limit > 0 && (static_cast<unsigned char>(error.message[limit]) & 0xC0) == 0x80)
            --limit;
        error.message.resize(limit);
    }
    return error;
}
} // namespace

std::string_view RuntimeStateName(RuntimeState state) noexcept
{
    switch (state)
    {
    case RuntimeState::Stopped:
        return "Stopped";
    case RuntimeState::Playing:
        return "Playing";
    case RuntimeState::Paused:
        return "Paused";
    case RuntimeState::Faulted:
        return "Faulted";
    }
    return "Stopped";
}
RuntimeSession::RuntimeSession(std::unique_ptr<Scene> scene, const InputState& input)
    : m_RuntimeScene(std::move(scene)), m_SourceInput(input)
{
}
Result<std::unique_ptr<RuntimeSession>>
RuntimeSession::Start(const Scene& editorScene, const ReflectionRegistry& reflection,
                      AssetService& assets, const InputState& input, bool startPaused,
                      const InputBindings& bindings, Viewport logicalViewport)
{
    auto cloned = SceneCloner::Clone(editorScene, reflection);
    if (!cloned)
        return Result<std::unique_ptr<RuntimeSession>>::Failure(cloned.GetError());
    auto session =
        std::unique_ptr<RuntimeSession>(new RuntimeSession(std::move(cloned).Value(), input));
    // A new session observes disk edits even when paused before its first tick.
    session->m_RuntimeScene->View<LuaScriptComponent>().ForEach(
        [&](ECS::Entity, LuaScriptComponent& script)
        {
            if (script.enabled)
                assets.Unload(script.script);
        });
    auto execution = RuntimeExecution::Create(*session->m_RuntimeScene, assets, input, bindings,
                                              logicalViewport);
    if (!execution)
        return Result<std::unique_ptr<RuntimeSession>>::Failure(execution.GetError());
    session->m_Execution = std::move(execution).Value();
    auto started = session->m_Execution->Start();
    if (!started)
        return Result<std::unique_ptr<RuntimeSession>>::Failure(
            BoundedRuntimeError(started.GetError()));
    session->m_Status.runtimeId = session->m_Execution->GetRuntimeId();
    session->m_Status.state = startPaused ? RuntimeState::Paused : RuntimeState::Playing;
    return Result<std::unique_ptr<RuntimeSession>>::Success(std::move(session));
}
RuntimeSession::~RuntimeSession()
{
    auto stopped = Stop();
    if (!stopped)
        JANUS_CORE_ERROR("Runtime shutdown: {}", stopped.GetError().message);
}
Result<void> RuntimeSession::Update(TimeStep timeStep)
{
    if (m_Status.state == RuntimeState::Stopped)
        return Result<void>::Failure(ErrorCode::InvalidState, "Runtime is stopped.");
    if (m_Status.state != RuntimeState::Playing)
        return Result<void>::Success();
    return Advance(timeStep, true);
}
Result<void> RuntimeSession::Advance(TimeStep timeStep, bool reload)
{
    if (!std::isfinite(timeStep.GetSeconds()))
        return Result<void>::Failure(ErrorCode::InvalidArgument, "Invalid runtime timestep.");
    auto result =
        reload ? m_Execution->Advance(timeStep, m_SourceInput)
               : m_Execution->Advance(timeStep, InputState{}, ScriptReloadPolicy::Skip, false);
    if (!result)
    {
        m_Status.state = RuntimeState::Faulted;
        m_Status.lastError = BoundedRuntimeError(result.GetError());
        m_Status.failedFrameIndex = m_Status.frameIndex + 1;
        m_Status.partialUpdate = true;
        return Result<void>::Failure(*m_Status.lastError);
    }
    ++m_Status.frameIndex;
    m_Status.lastStepSeconds = timeStep.GetSeconds();
    m_Status.simulationTimeSeconds += timeStep.GetSeconds();
    return Result<void>::Success();
}
Result<void> RuntimeSession::Pause()
{
    if (m_Status.state != RuntimeState::Playing && m_Status.state != RuntimeState::Paused)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Runtime cannot pause in this state.");
    m_Execution->CancelUI();
    m_Status.state = RuntimeState::Paused;
    return Result<void>::Success();
}
Result<void> RuntimeSession::Resume()
{
    if (m_Status.state != RuntimeState::Paused && m_Status.state != RuntimeState::Playing)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Runtime cannot resume; stop and restart.");
    if (m_Status.state == RuntimeState::Paused)
        m_Execution->PrimeUI(m_SourceInput);
    m_Status.state = RuntimeState::Playing;
    return Result<void>::Success();
}
Result<void> RuntimeSession::Step()
{
    if (m_Status.state != RuntimeState::Paused)
        return Result<void>::Failure(ErrorCode::InvalidState, "Pause before stepping.");
    return Advance(TimeStep::FromSeconds(1.0 / 60.0), false);
}
Result<void> RuntimeSession::Stop()
{
    if (m_Status.state == RuntimeState::Stopped)
        return Result<void>::Success();
    auto result = m_Execution->Stop();
    if (!result)
        m_Status.lastError = BoundedRuntimeError(result.GetError());
    m_Status.state = RuntimeState::Stopped;
    return result ? result : Result<void>::Failure(*m_Status.lastError);
}
bool RuntimeSession::IsRunning() const noexcept
{
    return m_Status.state != RuntimeState::Stopped;
}
const UIInteractionState& RuntimeSession::GetUIState() const noexcept
{
    return m_Execution->GetUIState();
}
RuntimeStatus RuntimeSession::GetStatus() const
{
    auto status = m_Status;
    status.entityCount = m_RuntimeScene->GetEntities().size();
    status.scriptInstanceCount = m_Execution ? m_Execution->InstanceCount() : 0;
    return status;
}
std::optional<ScriptSnapshot> RuntimeSession::GetSnapshot() const
{
    return m_Execution ? m_Execution->GetSnapshot() : std::nullopt;
}
Scene& RuntimeSession::GetScene() noexcept
{
    return *m_RuntimeScene;
}
const Scene& RuntimeSession::GetScene() const noexcept
{
    return *m_RuntimeScene;
}
} // namespace Janus
