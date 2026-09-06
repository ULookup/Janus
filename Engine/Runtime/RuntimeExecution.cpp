#include "Runtime/RuntimeExecution.h"

#include "Core/Log/Log.h"
#include "Scripting/ScriptEngine.h"

#include <cmath>
#include <utility>

namespace Janus
{
RuntimeExecution::RuntimeExecution(const InputState& initialInput) : m_Input(initialInput) {}

Result<std::unique_ptr<RuntimeExecution>> RuntimeExecution::Create(Scene& scene,
                                                                   AssetService& assets,
                                                                   const InputState& initialInput,
                                                                   const InputBindings& bindings)
{
    auto execution = std::unique_ptr<RuntimeExecution>(new RuntimeExecution(initialInput));
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
    return m_ScriptEngine->Start();
}

Result<void> RuntimeExecution::Advance(TimeStep timeStep, const InputState& input,
                                       ScriptReloadPolicy reload)
{
    if (!std::isfinite(timeStep.GetSeconds()))
        return Result<void>::Failure(ErrorCode::InvalidArgument, "Invalid runtime timestep.");
    if (!IsRunning())
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Runtime execution must be started before Advance.");
    m_Input = input;
    if (reload == ScriptReloadPolicy::CheckForChanges)
    {
        auto reloaded = m_ScriptEngine->ReloadChangedScripts();
        if (!reloaded)
            return reloaded;
    }
    return m_ScriptEngine->Update(timeStep);
}

Result<void> RuntimeExecution::Stop()
{
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
} // namespace Janus
