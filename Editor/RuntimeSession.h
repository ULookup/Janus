#pragma once

#include "Core/Error/Result.h"
#include "Core/Time/TimeStep.h"

#include <memory>

namespace Janus
{

class AssetService;
class InputState;
class Scene;
class ScriptEngine;

namespace Editor
{

class RuntimeSession final
{
public:
    [[nodiscard]] static Result<std::unique_ptr<RuntimeSession>> Start(
        const Scene& editorScene,
        AssetService& assets,
        const InputState& input);

    ~RuntimeSession();

    RuntimeSession(const RuntimeSession&) = delete;
    RuntimeSession& operator=(const RuntimeSession&) = delete;
    RuntimeSession(RuntimeSession&&) = delete;
    RuntimeSession& operator=(RuntimeSession&&) = delete;

    [[nodiscard]] Result<void> Update(TimeStep timeStep);
    [[nodiscard]] Result<void> Stop();

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] Scene& GetScene() noexcept;
    [[nodiscard]] const Scene& GetScene() const noexcept;

private:
    RuntimeSession(
        std::unique_ptr<Scene> runtimeScene,
        std::unique_ptr<ScriptEngine> scriptEngine) noexcept;

    std::unique_ptr<Scene> m_RuntimeScene;
    std::unique_ptr<ScriptEngine> m_ScriptEngine;
    bool m_Running = true;
};

} // namespace Editor
} // namespace Janus
