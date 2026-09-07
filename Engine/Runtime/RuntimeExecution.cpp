#include "Runtime/RuntimeExecution.h"

#include "Core/Log/Log.h"
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
                         const InputBindings& bindings, Viewport logicalViewport)
{
    auto execution = std::unique_ptr<RuntimeExecution>(
        new RuntimeExecution(scene, initialInput, logicalViewport));
    auto scripts = ScriptEngine::Create(scene, assets, execution->m_Input, bindings);
    if (!scripts)
        return Result<std::unique_ptr<RuntimeExecution>>::Failure(scripts.GetError());
    execution->m_ScriptEngine = std::move(scripts).Value();
    return Result<std::unique_ptr<RuntimeExecution>>::Success(std::move(execution));
}

RuntimeExecution::~RuntimeExecution()
{
    // Run OnDestroy while the borrowed Scene/assets and owned input are still alive.
    auto stopped = Stop();
    if (!stopped)
        JANUS_CORE_ERROR("Runtime execution shutdown: {}", stopped.GetError().message);
}

Result<void> RuntimeExecution::Start()
{
    if (IsRunning())
        return Result<void>::Failure(ErrorCode::InvalidState, "Runtime is already started.");
    m_RuntimeId = UUID::Random();
    m_FrameIndex = 0;
    m_ScriptEngine->SetSnapshotContext(m_RuntimeId, 0);
    return m_ScriptEngine->Start();
}

Result<void> RuntimeExecution::Advance(TimeStep timeStep, const InputState& input,
                                       ScriptReloadPolicy reload, bool dispatchUI)
{
    if (!std::isfinite(timeStep.GetSeconds()))
        return Result<void>::Failure(ErrorCode::InvalidArgument, "Invalid runtime timestep.");
    if (!IsRunning())
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Runtime execution must be started before Advance.");
    std::vector<UUID> clicks;
    if (dispatchUI)
    {
        auto layout = UILayout::Build(m_Scene, m_LogicalViewport);
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
    // Publications during reload, clicks and Update share the attempted simulation frame.
    m_ScriptEngine->SetSnapshotContext(m_RuntimeId, ++m_FrameIndex);
    if (reload == ScriptReloadPolicy::CheckForChanges)
    {
        auto reloaded = m_ScriptEngine->ReloadChangedScripts();
        if (!reloaded)
            return reloaded;
    }
    for (const auto id : clicks)
    {
        auto dispatched = m_ScriptEngine->DispatchButtonClick(id);
        if (!dispatched)
            return dispatched;
    }
    return m_ScriptEngine->Update(timeStep);
}

Result<void> RuntimeExecution::Stop()
{
    m_UI.Cancel();
    return m_ScriptEngine ? m_ScriptEngine->Stop() : Result<void>::Success();
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
