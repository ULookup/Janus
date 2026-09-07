#pragma once

#include "Core/Error/Result.h"
#include "Core/Input/InputActions.h"
#include "Core/Time/TimeStep.h"
#include "Core/Types.h"
#include "Core/UUID/UUID.h"
#include "Diagnostics/ScriptSnapshot.h"

#include <memory>
#include <optional>

namespace Janus
{

class AssetService;
class InputState;
class Scene;
class AnimationSystem;

class ScriptEngine final
{
public:
  [[nodiscard]] static Result<std::unique_ptr<ScriptEngine>>
  Create(Scene& scene, AssetService& assets, const InputState& input,
         const InputBindings& bindings = {});

  ~ScriptEngine();

  ScriptEngine(const ScriptEngine&) = delete;
  ScriptEngine& operator=(const ScriptEngine&) = delete;
  ScriptEngine(ScriptEngine&&) = delete;
  ScriptEngine& operator=(ScriptEngine&&) = delete;

  [[nodiscard]] Result<void> Start();
  [[nodiscard]] Result<void> ReloadChangedScripts();
  [[nodiscard]] Result<void> Update(TimeStep timeStep);
  [[nodiscard]] Result<void> DispatchButtonClick(UUID entity);
  [[nodiscard]] Result<void> Stop();

  [[nodiscard]] bool IsRunning() const noexcept;
  [[nodiscard]] usize InstanceCount() const noexcept;
  [[nodiscard]] std::optional<ScriptSnapshot> GetSnapshot() const;

private:
  friend class RuntimeExecution;
  void SetAnimations(AnimationSystem* animations);
  void SetSnapshotContext(UUID runtimeId, u64 frameIndex);
  struct Impl;

  explicit ScriptEngine(std::unique_ptr<Impl> impl) noexcept;

  std::unique_ptr<Impl> m_Impl;
};

} // namespace Janus
