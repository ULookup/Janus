#pragma once

#include "Application/ApplicationConfig.h"
#include "Asset/AssetRegistry.h"
#include "Core/Command/CommandBus.h"
#include "Core/Error/Result.h"
#include "Core/Input/InputState.h"
#include "Core/Log/LogStore.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/Time/TimeStep.h"
#include "Diagnostics/DiagnosticsFrame.h"
#include "Project/ProjectSettings.h"
#include "Runtime/RuntimeSession.h"

#include <chrono>
#include <filesystem>
#include <memory>

namespace Janus
{

class AssetService;
class InputState;
class Renderer2D;
class Scene;

namespace Editor
{

using RuntimeSession = Janus::RuntimeSession;

class ProjectSession final
{
public:
  [[nodiscard]] static Result<std::unique_ptr<ProjectSession>>
  Open(const ProjectRuntimeConfig& config, Renderer2D& renderer,
       std::shared_ptr<LogStore> logs = {});

  ~ProjectSession();

  ProjectSession(const ProjectSession&) = delete;
  ProjectSession& operator=(const ProjectSession&) = delete;
  ProjectSession(ProjectSession&&) = delete;
  ProjectSession& operator=(ProjectSession&&) = delete;

  [[nodiscard]] const ProjectSettings& GetProjectSettings() const noexcept
  {
      return m_Settings;
  }
  [[nodiscard]] Result<void> SaveProjectSettings(const ProjectSettings& settings);
  [[nodiscard]] Result<AssetHandle> ExportPrefab(UUID root);
  [[nodiscard]] const std::filesystem::path& GetProjectRoot() const noexcept;
  [[nodiscard]] const std::filesystem::path& GetCurrentScenePath() const noexcept;
  [[nodiscard]] const AssetRegistry& GetAssetRegistry() const noexcept;
  [[nodiscard]] ReflectionRegistry& GetReflectionRegistry() noexcept;
  [[nodiscard]] const ReflectionRegistry& GetReflectionRegistry() const noexcept;
  [[nodiscard]] CommandBus& GetCommandBus() noexcept;
  [[nodiscard]] const CommandBus& GetCommandBus() const noexcept;
  [[nodiscard]] AssetService& GetAssetService() noexcept;
  [[nodiscard]] Scene& GetEditorScene() noexcept;
  [[nodiscard]] const Scene& GetEditorScene() const noexcept;

  [[nodiscard]] Result<void> StartRuntime(const InputState& input, bool startPaused = false);
  Result<void> ExecuteAuthoring(std::unique_ptr<ICommand> command,
                                CommandActor actor = CommandActor::Human, UUID token = {},
                                UUID owner = {});
  Result<void> UndoAuthoring();
  Result<void> RedoAuthoring();
  Result<UUID> BeginAuthoringTransaction(UUID owner, std::string label = {});
  Result<void> FinishAuthoringTransaction(UUID token, UUID owner, bool commit);
  void CancelAuthoringTransaction(UUID owner, CommandActor actor = CommandActor::System);
  void ExpireAuthoringTransaction(
      std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
  bool IsAuthoringReadOnly() const noexcept;
  Result<void> DiscardUnsavedAndReload();
  u64 GetSceneRevision() const noexcept
  {
      return m_SceneRevision;
  }
  UUID GetTransactionOwner() const noexcept
  {
      return m_TransactionOwner;
  }
  CpuProfiler& GetProfiler() noexcept
  {
      return m_Profiler;
  }
  bool BeginDiagnosticsFrame();
  void CaptureRenderPass(bool game, const RendererStatistics& stats, usize entities);
  void EndDiagnosticsFrame();
  std::optional<DiagnosticsFrame> FindDiagnosticsFrame(u64 id) const;
  const std::optional<DiagnosticsFrame>& GetDiagnosticsFrame() const noexcept
  {
      return m_Diagnostics;
  }
  [[nodiscard]] Result<void> PlayRuntime(bool startPaused = false);
  [[nodiscard]] Result<void> PauseRuntime();
  [[nodiscard]] Result<void> ResumeRuntime();
  [[nodiscard]] Result<void> StepRuntime();
  [[nodiscard]] bool HasRuntime() const noexcept;
  [[nodiscard]] RuntimeState GetRuntimeState() const noexcept;
  [[nodiscard]] RuntimeStatus GetRuntimeStatus() const;
  [[nodiscard]] std::shared_ptr<LogStore> GetLogStore() const noexcept
  {
      return m_Logs;
  }
    [[nodiscard]] Result<void> UpdateRuntime(TimeStep timeStep);
    [[nodiscard]] Result<void> StopRuntime();

    [[nodiscard]] bool IsPlaying() const noexcept;
    [[nodiscard]] bool IsDirty() const noexcept;
    void MarkDirty() noexcept;
    [[nodiscard]] Result<void> SaveCurrentScene();

    [[nodiscard]] RuntimeSession* GetRuntimeSession() noexcept;
    [[nodiscard]] const RuntimeSession* GetRuntimeSession() const noexcept;

private:
  ProjectSession(std::filesystem::path projectRoot, std::filesystem::path currentScenePath,
                 ReflectionRegistry reflectionRegistry, AssetRegistry assetRegistry,
                 std::unique_ptr<Scene> editorScene, Renderer2D& renderer,
                 std::shared_ptr<LogStore> logs);

  ProjectSettings m_Settings;
  std::filesystem::path m_ProjectRoot;
  std::filesystem::path m_CurrentScenePath;
  ReflectionRegistry m_ReflectionRegistry;
  AssetRegistry m_AssetRegistry;
  std::unique_ptr<AssetService> m_AssetService;
  std::unique_ptr<Scene> m_EditorScene;

  // Commands retain references into the authoring capability graph.
  // Keep history after those dependencies in declaration order so it
  // is destroyed before Scene, AssetRegistry, and ReflectionRegistry.
  CommandBus m_CommandBus;
  CpuProfiler m_Profiler;
  std::unique_ptr<RuntimeSession> m_RuntimeSession;
  bool m_Dirty = false;
  RuntimeStatus m_LastRuntimeStatus;
  InputState m_AgentInput;
  UUID m_TransactionOwner;
  bool m_DirtyBeforeTransaction = false;
  std::chrono::steady_clock::time_point m_TransactionDeadline;
  std::deque<std::pair<UUID, UUID>> m_TransactionOwners;
  u64 m_SceneRevision = 0;
  DiagnosticsFrame m_PendingDiagnostics;
  std::optional<DiagnosticsFrame> m_Diagnostics;
  std::deque<DiagnosticsFrame> m_DiagnosticsHistory;
  std::shared_ptr<LogStore> m_Logs;
  void RecordRuntimeResult(const Result<void>& result);
};

} // namespace Editor
} // namespace Janus
