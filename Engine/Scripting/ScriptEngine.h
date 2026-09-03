#pragma once

#include "Core/Error/Result.h"
#include "Core/Time/TimeStep.h"
#include "Core/Types.h"

#include <memory>

namespace Janus
{

class AssetService;
class InputState;
class Scene;

class ScriptEngine final
{
public:
    [[nodiscard]] static Result<std::unique_ptr<ScriptEngine>> Create(
        Scene& scene,
        AssetService& assets,
        const InputState& input);

    ~ScriptEngine();

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;
    ScriptEngine(ScriptEngine&&) = delete;
    ScriptEngine& operator=(ScriptEngine&&) = delete;

    [[nodiscard]] Result<void> Start();
    [[nodiscard]] Result<void> Update(TimeStep timeStep);
    [[nodiscard]] Result<void> Stop();

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] usize InstanceCount() const noexcept;

private:
    struct Impl;

    explicit ScriptEngine(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> m_Impl;
};

} // namespace Janus
