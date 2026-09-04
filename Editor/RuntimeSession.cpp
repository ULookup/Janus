#include "RuntimeSession.h"

#include "Asset/AssetService.h"
#include "Core/Input/InputState.h"
#include "Core/Log/Log.h"
#include "Scene/Scene.h"
#include "Scene/SceneCloner.h"
#include "Scripting/ScriptEngine.h"

#include <utility>

namespace Janus::Editor
{

Result<std::unique_ptr<RuntimeSession>> RuntimeSession::Start(
    const Scene& editorScene,
    AssetService& assets,
    const InputState& input)
{
    auto clonedScene = SceneCloner::Clone(editorScene);
    if (!clonedScene)
    {
        return Result<std::unique_ptr<RuntimeSession>>::Failure(
            clonedScene.GetError());
    }

    std::unique_ptr<Scene> runtimeScene =
        std::move(clonedScene).Value();

    auto scriptEngine = ScriptEngine::Create(
        *runtimeScene,
        assets,
        input);
    if (!scriptEngine)
    {
        return Result<std::unique_ptr<RuntimeSession>>::Failure(
            scriptEngine.GetError());
    }

    std::unique_ptr<ScriptEngine> scripts =
        std::move(scriptEngine).Value();

    auto started = scripts->Start();
    if (!started)
    {
        return Result<std::unique_ptr<RuntimeSession>>::Failure(
            started.GetError());
    }

    return Result<std::unique_ptr<RuntimeSession>>::Success(
        std::unique_ptr<RuntimeSession>(
            new RuntimeSession(
                std::move(runtimeScene),
                std::move(scripts))));
}

RuntimeSession::RuntimeSession(
    std::unique_ptr<Scene> runtimeScene,
    std::unique_ptr<ScriptEngine> scriptEngine) noexcept
    : m_RuntimeScene(std::move(runtimeScene)),
      m_ScriptEngine(std::move(scriptEngine))
{
}

RuntimeSession::~RuntimeSession()
{
    const auto stopped = Stop();
    if (!stopped)
    {
        JANUS_CORE_ERROR(
            "RuntimeSession shutdown failed: {}",
            stopped.GetError().message);
    }
}

Result<void> RuntimeSession::Update(TimeStep timeStep)
{
    if (!m_Running)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "RuntimeSession must be running before Update.");
    }

    auto reloaded = m_ScriptEngine->ReloadChangedScripts();
    if (!reloaded)
    {
        return reloaded;
    }

    return m_ScriptEngine->Update(timeStep);
}

Result<void> RuntimeSession::Stop()
{
    if (!m_Running)
    {
        return Result<void>::Success();
    }

    auto stopped = m_ScriptEngine->Stop();
    m_Running = false;
    return stopped;
}

bool RuntimeSession::IsRunning() const noexcept
{
    return m_Running;
}

Scene& RuntimeSession::GetScene() noexcept
{
    return *m_RuntimeScene;
}

const Scene& RuntimeSession::GetScene() const noexcept
{
    return *m_RuntimeScene;
}

} // namespace Janus::Editor
