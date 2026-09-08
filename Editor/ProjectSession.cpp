#include "ProjectSession.h"

#include "RuntimeSession.h"

#include "Asset/AssetService.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Input/InputState.h"
#include "Prefab/Prefab.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace Janus::Editor
{
Result<std::unique_ptr<ProjectSession>> ProjectSession::Open(const ProjectRuntimeConfig& config,
                                                             Renderer2D& renderer,
                                                             std::shared_ptr<LogStore> logs)
{
    auto settings = LoadProjectSettings(config);
    if (!settings)
        return Result<std::unique_ptr<ProjectSession>>::Failure(settings.GetError());
    auto registryPath = ResolveProjectPath(config.root, settings.Value().assetRegistry);
    if (!registryPath)
    {
        return Result<std::unique_ptr<ProjectSession>>::Failure(
            registryPath.GetError());
    }

    auto scenePath = ResolveProjectPath(config.root, settings.Value().defaultScene);
    if (!scenePath)
    {
        return Result<std::unique_ptr<ProjectSession>>::Failure(
            scenePath.GetError());
    }

    auto registry = AssetRegistry::Load(registryPath.Value());
    if (!registry)
    {
        return Result<std::unique_ptr<ProjectSession>>::Failure(
            registry.GetError());
    }

    auto reflection = CreateBuiltinSceneReflectionRegistry();
    if (!reflection)
    {
        return Result<std::unique_ptr<ProjectSession>>::Failure(
            reflection.GetError());
    }

    auto scene = SceneDeserializer::Load(
        scenePath.Value(),
        reflection.Value());
    if (!scene)
    {
        return Result<std::unique_ptr<ProjectSession>>::Failure(
            scene.GetError());
    }

    auto session = std::unique_ptr<ProjectSession>(
        new ProjectSession(config.root, settings.Value().defaultScene.lexically_normal(),
                           std::move(reflection).Value(), std::move(registry).Value(),
                           std::move(scene).Value(), renderer, std::move(logs)));

    session->m_Settings = std::move(settings).Value();
    return Result<std::unique_ptr<ProjectSession>>::Success(std::move(session));
}

ProjectSession::ProjectSession(std::filesystem::path projectRoot,
                               std::filesystem::path currentScenePath,
                               ReflectionRegistry reflectionRegistry, AssetRegistry assetRegistry,
                               std::unique_ptr<Scene> editorScene, Renderer2D& renderer,
                               std::shared_ptr<LogStore> logs)
    : m_ProjectRoot(std::move(projectRoot)), m_CurrentScenePath(std::move(currentScenePath)),
      m_ReflectionRegistry(std::move(reflectionRegistry)),
      m_AssetRegistry(std::move(assetRegistry)),
      m_AssetService(std::make_unique<AssetService>(m_ProjectRoot, m_AssetRegistry, renderer)),
      m_EditorScene(std::move(editorScene)),
      m_Logs(logs ? std::move(logs) : std::make_shared<LogStore>())
{
}

ProjectSession::~ProjectSession() = default;

Result<void> ProjectSession::ValidatePrefabName(std::string_view name)
{
    auto invalid = []
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Prefab name must be valid UTF-8 (1..96 bytes), without path separators, controls, "
            "reserved names or trailing spaces/dots.");
    };
    if (name.empty() || name.size() > 96 || name.back() == '.' || name.back() == ' ')
        return invalid();
    for (unsigned char c : name)
        if (c < 32 || c == 127 ||
            std::string_view("<>:\"/\\|?*").find(static_cast<char>(c)) != std::string_view::npos)
            return invalid();
    const std::string text(name);
    auto encoded =
        nlohmann::json(text).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    if (nlohmann::json::parse(encoded).get<std::string>() != text)
        return invalid();
    for (usize i = 0; i + 1 < name.size(); ++i)
        if (static_cast<unsigned char>(name[i]) == 0xc2 &&
            static_cast<unsigned char>(name[i + 1]) >= 0x80 &&
            static_cast<unsigned char>(name[i + 1]) <= 0x9f)
            return invalid(); // Unicode C1 controls are also unsuitable for display names.
    auto stem = text.substr(0, text.find('.'));
    while (!stem.empty() && stem.back() == ' ')
        stem.pop_back();
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c)
                   { return static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c); });
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" || stem == "CONIN$" ||
        stem == "CONOUT$" ||
        ((stem.starts_with("COM") || stem.starts_with("LPT")) &&
         ((stem.size() == 4 && stem[3] >= '1' && stem[3] <= '9') || stem.substr(3) == "\xc2\xb9" ||
          stem.substr(3) == "\xc2\xb2" || stem.substr(3) == "\xc2\xb3")))
        return invalid();
    return Result<void>::Success();
}

Result<AssetHandle> ProjectSession::ExportPrefab(UUID root, std::optional<std::string> name)
{
    if (IsAuthoringReadOnly())
        return Result<AssetHandle>::Failure(
            ErrorCode::InvalidState,
            "Stop runtime and finish transaction/recovery before exporting a Prefab.");
    if (name)
    {
        auto valid = ValidatePrefabName(*name);
        if (!valid)
            return Result<AssetHandle>::Failure(valid.GetError());
    }
    auto text = Prefab::Capture(*m_EditorScene, root, m_ReflectionRegistry);
    if (!text)
        return Result<AssetHandle>::Failure(text.GetError());
    const AssetHandle handle{UUID::Random()};
    const auto filename = (name ? *name + "-" : "") + handle.ToString() + ".prefab";
    const auto relative = std::filesystem::path("Prefabs") /
                          std::filesystem::path(std::u8string(filename.begin(), filename.end()));
    auto path = ResolveProjectPath(m_ProjectRoot, relative);
    if (!path)
        return Result<AssetHandle>::Failure(path.GetError());
    auto registryPath = ResolveProjectPath(m_ProjectRoot, m_Settings.assetRegistry);
    if (!registryPath)
        return Result<AssetHandle>::Failure(registryPath.GetError());
    AssetRegistry next = m_AssetRegistry;
    auto registered = next.Register(AssetMetadata{handle, AssetType::Prefab, relative});
    if (!registered)
        return Result<AssetHandle>::Failure(registered.GetError());
    std::error_code error;
    const bool exists = std::filesystem::exists(path.Value(), error);
    if (error || exists)
        return Result<AssetHandle>::Failure(ErrorCode::FileWriteFailed,
                                            "Prefab destination is unavailable.");
    std::filesystem::create_directories(path.Value().parent_path(), error);
    if (error)
        return Result<AssetHandle>::Failure(ErrorCode::FileWriteFailed, error.message());
    auto written = FileSystem::WriteTextAtomic(path.Value(), text.Value(),
                                               FileSystem::AtomicWriteMode::CreateNew);
    if (!written)
        return Result<AssetHandle>::Failure(written.GetError());
    auto saved = next.Save(registryPath.Value());
    if (!saved)
    {
        // Only this fresh UUID file belongs to the failed export. Existing assets are untouched.
        std::filesystem::remove(path.Value(), error);
        if (error)
            return Result<AssetHandle>::Failure(
                ErrorCode::FileWriteFailed,
                saved.GetError().message +
                    " Unregistered Prefab cleanup failed: " + error.message());
        return Result<AssetHandle>::Failure(saved.GetError());
    }
    m_AssetRegistry = std::move(next);
    return Result<AssetHandle>::Success(handle);
}

Result<void> ProjectSession::SaveProjectSettings(const ProjectSettings& settings)
{
    if (m_ClosePending)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Editor close confirmation is active.");
    return SaveProjectSettingsImpl(settings);
}

Result<void> ProjectSession::SaveProjectSettingsImpl(const ProjectSettings& settings)
{
    if (HasRuntime() || m_CommandBus.HasTransaction() || m_CommandBus.RecoveryRequired())
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Stop runtime and finish transaction/recovery before saving project settings.");
    auto saved = Janus::SaveProjectSettings(m_ProjectRoot, settings);
    if (!saved)
        return saved;
    m_Settings = settings;
    return Result<void>::Success();
}

const std::filesystem::path& ProjectSession::GetProjectRoot() const noexcept
{
    return m_ProjectRoot;
}

const std::filesystem::path& ProjectSession::GetCurrentScenePath() const noexcept
{
    return m_CurrentScenePath;
}

const AssetRegistry& ProjectSession::GetAssetRegistry() const noexcept
{
    return m_AssetRegistry;
}

ReflectionRegistry& ProjectSession::GetReflectionRegistry() noexcept
{
    return m_ReflectionRegistry;
}

const ReflectionRegistry& ProjectSession::GetReflectionRegistry() const noexcept
{
    return m_ReflectionRegistry;
}

CommandBus& ProjectSession::GetCommandBus() noexcept
{
    return m_CommandBus;
}

const CommandBus& ProjectSession::GetCommandBus() const noexcept
{
    return m_CommandBus;
}

AssetService& ProjectSession::GetAssetService() noexcept
{
    return *m_AssetService;
}

Scene& ProjectSession::GetEditorScene() noexcept
{
    return *m_EditorScene;
}

const Scene& ProjectSession::GetEditorScene() const noexcept
{
    return *m_EditorScene;
}

Result<void> ProjectSession::StartRuntime(const InputState& input, bool startPaused)
{
    if (m_ClosePending)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Editor close confirmation is active.");
    if (m_RuntimeSession != nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "ProjectSession already has an active RuntimeSession.");
    }

    if (m_CommandBus.HasTransaction() || m_CommandBus.RecoveryRequired())
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Finish authoring transaction or recovery before Play.");
    auto runtime = RuntimeSession::Start(*m_EditorScene, m_ReflectionRegistry, *m_AssetService,
                                         input, startPaused, m_Settings.inputBindings,
                                         {m_Settings.width, m_Settings.height}, {}, &m_Profiler);
    if (!runtime)
    {
        m_LastRuntimeStatus = {};
        m_LastRuntimeStatus.lastError = runtime.GetError();
        RecordRuntimeResult(Result<void>::Failure(runtime.GetError()));
        return Result<void>::Failure(runtime.GetError());
    }

    m_RuntimeSession = std::move(runtime).Value();
    m_Logs->Append(LogLevel::Info, "Runtime", "Runtime started.",
                   {m_RuntimeSession->GetStatus().runtimeId});
    return Result<void>::Success();
}

Result<void> ProjectSession::UpdateRuntime(TimeStep timeStep)
{
    if (m_RuntimeSession == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "ProjectSession has no active RuntimeSession.");
    }

    const bool ownFrame = BeginDiagnosticsFrame();
    auto result = [&]
    {
        CpuScope scope(m_Profiler, "Runtime.Update");
        return m_RuntimeSession->Update(timeStep);
    }();
    if (ownFrame)
        EndDiagnosticsFrame();
    RecordRuntimeResult(result);
    return result;
}

Result<void> ProjectSession::StopRuntime()
{
    if (m_ClosePending)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Editor close confirmation is active.");
    return StopRuntimeImpl();
}

Result<void> ProjectSession::StopRuntimeImpl()
{
    if (m_RuntimeSession == nullptr)
    {
        return Result<void>::Success();
    }

    auto stopped = m_RuntimeSession->Stop();
    RecordRuntimeResult(stopped);
    m_LastRuntimeStatus = m_RuntimeSession->GetStatus();
    m_LastRuntimeStatus.runtimeId = {};
    m_LastRuntimeStatus.entityCount = 0;
    m_LastRuntimeStatus.scriptInstanceCount = 0;
    m_RuntimeSession.reset();
    return stopped;
}

bool ProjectSession::IsPlaying() const noexcept
{
    return HasRuntime();
}

bool ProjectSession::HasRuntime() const noexcept
{
    return m_RuntimeSession != nullptr;
}
RuntimeState ProjectSession::GetRuntimeState() const noexcept
{
    return m_RuntimeSession ? m_RuntimeSession->GetState() : RuntimeState::Stopped;
}
RuntimeStatus ProjectSession::GetRuntimeStatus() const
{
    return m_RuntimeSession ? m_RuntimeSession->GetStatus() : m_LastRuntimeStatus;
}
Result<void> ProjectSession::PlayRuntime(bool startPaused)
{
    if (!HasRuntime())
        return StartRuntime(m_AgentInput, startPaused);
    if (startPaused)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "startPaused requires a stopped runtime.");
    return ResumeRuntime();
}

Result<void> ProjectSession::PauseRuntime()
{
    if (m_ClosePending)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Editor close confirmation is active.");
    return m_RuntimeSession
               ? m_RuntimeSession->Pause()
               : Result<void>::Failure(ErrorCode::InvalidState, "No runtime to pause.");
}
Result<void> ProjectSession::ResumeRuntime()
{
    if (m_ClosePending)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Editor close confirmation is active.");
    return m_RuntimeSession
               ? m_RuntimeSession->Resume()
               : Result<void>::Failure(ErrorCode::InvalidState, "No runtime to resume.");
}
Result<void> ProjectSession::StepRuntime()
{
    if (m_ClosePending)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Editor close confirmation is active.");
    const bool ownFrame = BeginDiagnosticsFrame();
    auto result = m_RuntimeSession
                      ? [&]
    {
        CpuScope scope(m_Profiler, "Runtime.Step");
        return m_RuntimeSession->Step();
    }()
                      : Result<void>::Failure(ErrorCode::InvalidState, "No runtime to step.");
    if (ownFrame)
        EndDiagnosticsFrame();
    RecordRuntimeResult(result);
    return result;
}

bool ProjectSession::BeginDiagnosticsFrame()
{
    if (!m_Profiler.BeginFrame())
        return false;
    m_PendingDiagnostics = {};
    return true;
}
void ProjectSession::CaptureRenderPass(bool game, const RendererStatistics& stats, usize entities)
{
    if (m_Profiler.IsRecording())
        (game ? m_PendingDiagnostics.gameView : m_PendingDiagnostics.sceneView) =
            RenderPassSnapshot{stats, entities};
}
void ProjectSession::EndDiagnosticsFrame()
{
    if (!m_Profiler.EndFrame())
        return;
    m_PendingDiagnostics.cpu = *m_Profiler.Latest();
    const auto runtime = GetRuntimeStatus();
    m_PendingDiagnostics.runtimeId = runtime.runtimeId;
    m_PendingDiagnostics.simulationFrame = runtime.frameIndex;
    m_Diagnostics = std::move(m_PendingDiagnostics);
    if (m_DiagnosticsHistory.size() == 120)
        m_DiagnosticsHistory.pop_front();
    m_DiagnosticsHistory.push_back(*m_Diagnostics);
}

std::optional<DiagnosticsFrame> ProjectSession::FindDiagnosticsFrame(u64 id) const
{
    for (const auto& frame : m_DiagnosticsHistory)
        if (frame.cpu.frameId == id)
            return frame;
    return {};
}

void ProjectSession::RecordRuntimeResult(const Result<void>& result)
{
    if (result)
        return;
    const auto status = GetRuntimeStatus();
    m_Logs->Append(LogLevel::Error, "Runtime", result.GetError().message,
                   {status.runtimeId,
                    status.partialUpdate ? status.failedFrameIndex : status.frameIndex,
                    result.GetError().code});
}

bool ProjectSession::IsDirty() const noexcept
{
    return m_Dirty;
}

void ProjectSession::MarkDirty() noexcept
{
    m_Dirty = true;
}

Result<void> ProjectSession::SaveCurrentScene()
{
    if (m_ClosePending)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Editor close confirmation is active.");
    return SaveCurrentSceneImpl();
}

Result<void> ProjectSession::SaveCurrentSceneImpl()
{
    if (HasRuntime() || m_CommandBus.HasTransaction() || m_CommandBus.RecoveryRequired())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Cannot save the authoring Scene while Play Mode is active.");
    }

    const std::filesystem::path scenePath =
        m_ProjectRoot / m_CurrentScenePath;

    auto saved =
        SceneSerializer::Save(
            *m_EditorScene,
            m_ReflectionRegistry,
            scenePath);
    if (!saved)
    {
        return saved;
    }

    m_Dirty = false;
    return Result<void>::Success();
}

RuntimeSession* ProjectSession::GetRuntimeSession() noexcept
{
    return m_RuntimeSession.get();
}

const RuntimeSession* ProjectSession::GetRuntimeSession() const noexcept
{
    return m_RuntimeSession.get();
}

bool ProjectSession::IsAuthoringReadOnly() const noexcept
{
    return m_ClosePending || HasRuntime() || m_CommandBus.HasTransaction() ||
           m_CommandBus.RecoveryRequired();
}
Result<void> ProjectSession::ExecuteAuthoring(std::unique_ptr<ICommand> command, CommandActor actor,
                                              UUID token, UUID owner)
{
    ExpireAuthoringTransaction();
    if (m_ClosePending || HasRuntime() || m_CommandBus.RecoveryRequired())
        return Result<void>::Failure(ErrorCode::InvalidState, "Authoring is read-only.");
    if (m_CommandBus.HasTransaction() && (owner != m_TransactionOwner || !owner.IsValid()))
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Authoring transaction belongs to another writer.");
    const bool pending = m_CommandBus.HasTransaction();
    auto result = m_CommandBus.Execute(std::move(command), actor, token, owner);
    if (result)
        m_Dirty = true;
    if (pending && !m_CommandBus.HasTransaction() && !m_CommandBus.RecoveryRequired())
        m_Dirty = m_DirtyBeforeTransaction;
    return result;
}
Result<void> ProjectSession::UndoAuthoring()
{
    if (IsAuthoringReadOnly())
        return Result<void>::Failure(ErrorCode::InvalidState, "Authoring is read-only.");
    auto result = m_CommandBus.Undo();
    if (result)
        m_Dirty = true;
    return result;
}
Result<void> ProjectSession::RedoAuthoring()
{
    if (IsAuthoringReadOnly())
        return Result<void>::Failure(ErrorCode::InvalidState, "Authoring is read-only.");
    auto result = m_CommandBus.Redo();
    if (result)
        m_Dirty = true;
    return result;
}
Result<UUID> ProjectSession::BeginAuthoringTransaction(UUID owner, std::string label)
{
    ExpireAuthoringTransaction();
    if (!owner.IsValid() || IsAuthoringReadOnly())
        return Result<UUID>::Failure(ErrorCode::InvalidState, "Authoring transaction unavailable.");
    auto token = m_CommandBus.BeginTransaction(CommandActor::Agent, std::move(label));
    if (!token)
        return token;
    m_TransactionOwner = owner;
    m_DirtyBeforeTransaction = m_Dirty;
    m_TransactionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    if (m_TransactionOwners.size() == 32)
        m_TransactionOwners.pop_front();
    m_TransactionOwners.emplace_back(token.Value(), owner);
    return token;
}
Result<void> ProjectSession::FinishAuthoringTransaction(UUID token, UUID owner, bool commit)
{
    ExpireAuthoringTransaction();
    bool known = false;
    for (const auto& pair : m_TransactionOwners)
        if (pair.first == token && pair.second == owner)
            known = true;
    if (!known)
        return Result<void>::Failure(ErrorCode::InvalidState, "Unknown transaction owner/token.");
    const bool active = m_CommandBus.HasTransaction() && m_CommandBus.GetTransactionId() == token;
    auto result = commit ? m_CommandBus.CommitTransaction(token)
                         : m_CommandBus.RollbackTransaction(token, CommandActor::Agent);
    if (active && result && !commit)
        m_Dirty = m_DirtyBeforeTransaction;
    return result;
}
void ProjectSession::CancelAuthoringTransaction(UUID owner, CommandActor actor)
{
    if (!m_CommandBus.HasTransaction() || owner != m_TransactionOwner)
        return;
    auto result = m_CommandBus.RollbackTransaction(m_CommandBus.GetTransactionId(), actor);
    if (result)
        m_Dirty = m_DirtyBeforeTransaction;
    else
        RecordRuntimeResult(result);
}
void ProjectSession::ExpireAuthoringTransaction(std::chrono::steady_clock::time_point now)
{
    if (m_CommandBus.HasTransaction() && !m_CommandBus.RecoveryRequired() &&
        now >= m_TransactionDeadline)
        CancelAuthoringTransaction(m_TransactionOwner);
}
Result<void> ProjectSession::DiscardUnsavedAndReload()
{
    if (m_ClosePending)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Cancel close confirmation before recovery.");
    if (HasRuntime())
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Stop runtime before discard/reload.");
    auto loaded = SceneDeserializer::Load(m_ProjectRoot / m_CurrentScenePath, m_ReflectionRegistry);
    if (!loaded)
        return Result<void>::Failure(loaded.GetError());
    // Destroy commands before their referenced scene. MCP refreshes bindings on this revision.
    m_CommandBus.ResetAfterRecovery();
    m_EditorScene = std::move(loaded).Value();
    m_Dirty = false;
    m_TransactionOwner = {};
    m_TransactionOwners.clear();
    ++m_SceneRevision;
    m_CommandBus.RecordOperation(CommandActor::Human, "Discard unsaved changes and reload",
                                 Result<void>::Success());
    return Result<void>::Success();
}

} // namespace Janus::Editor
