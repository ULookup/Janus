#include "EditorActions.h"
#include "EditorCloseController.h"
#include "EditorContext.h"
#include "ProjectSession.h"

#include "Application/ApplicationConfig.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Input/InputState.h"
#include "Project/ProjectSettings.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

TEST_CASE("Scene documents preserve old state on failed prepare and guard dirty replacement",
          "[editor][e1]")
{
    using namespace Janus;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    ProjectRuntimeConfig config;
    config.root = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "SandboxProject";
    auto opened = Editor::ProjectSession::Open(config, *renderer);
    REQUIRE(opened);
    auto& session = *opened.Value();
    const auto path = session.GetCurrentScenePath();
    auto* original = &session.GetEditorScene();
    REQUIRE_FALSE(session.PrepareOpenScene("../escape.scene"));
    REQUIRE_FALSE(session.PrepareOpenScene("Scenes/missing.scene"));
    CHECK(&session.GetEditorScene() == original);
    CHECK(session.GetCurrentScenePath() == path);
    auto candidate = session.PrepareNewScene("Scenes/E1-unsaved.scene");
    REQUIRE(candidate);
    session.MarkDirty();
    CHECK_FALSE(session.CommitPreparedScene(*candidate.Value()));
    CHECK_FALSE(session.OpenScene(path));
    CHECK(&session.GetEditorScene() == original);
    candidate = session.PrepareNewScene("Scenes/E1-unsaved.scene");
    REQUIRE(candidate);
    Editor::EditorCloseController leave(session);
    leave.Request();
    REQUIRE(leave.ConfirmSceneChange(*candidate.Value(), false, false));
    CHECK_FALSE(session.IsClosePending());
    CHECK(session.IsDirty());
    CHECK_FALSE(session.HasSavedSceneFile());
    CHECK(session.GetEditorScene().GetEntities().empty());
    CHECK(session.GetSceneRevision() == 1);
    REQUIRE(session.DiscardUnsavedAndReload());
    CHECK(session.GetEditorScene().GetEntities().empty());
    CHECK(session.IsDirty());
}

namespace
{

std::filesystem::path SandboxProjectRoot()
{
    return std::filesystem::path(JANUS_TEST_SOURCE_DIR)
        .parent_path()
        / "SandboxProject";
}

class ProjectTempDirectory final
{
public:
    ProjectTempDirectory()
    {
        const auto leaf =
            "janus-editor-project-"
            + std::to_string(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count());

        m_Path =
            std::filesystem::temp_directory_path()
            / leaf;

        std::error_code error;
        std::filesystem::copy(
            SandboxProjectRoot(),
            m_Path,
            std::filesystem::copy_options::recursive,
            error);

        if (error)
        {
            throw std::runtime_error(
                "Failed to copy SandboxProject for Editor tests.");
        }
    }

    ~ProjectTempDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_Path, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return m_Path;
    }

private:
    std::filesystem::path m_Path;
};

} // namespace

TEST_CASE("Unsaved scene recovery does not depend on its reserved disk path", "[editor][e1]")
{
    using namespace Janus;
    ProjectTempDirectory temp;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    auto opened = Editor::ProjectSession::Open({temp.Path()}, *renderer);
    REQUIRE(opened);
    auto& session = *opened.Value();
    REQUIRE(session.NewScene("Scenes/Reserved.scene"));
    Editor::EditorContext context{&session};
    Editor::EditorActions actions(context);
    REQUIRE(actions.CreateEntity("Discard me"));
    REQUIRE(std::filesystem::create_directory(temp.Path() / "Scenes/Reserved.scene"));
    CHECK_FALSE(session.SaveCurrentScene());
    REQUIRE(session.DiscardUnsavedAndReload());
    CHECK(session.GetEditorScene().GetEntities().empty());
    CHECK(session.GetCommandBus().GetHistorySize() == 0);
    CHECK(session.IsDirty());
    CHECK_FALSE(session.HasSavedSceneFile());
    CHECK(session.GetCurrentScenePath() == "Scenes/Reserved.scene");
    CHECK(std::filesystem::is_directory(temp.Path() / "Scenes/Reserved.scene"));
    REQUIRE(session.SaveSceneAs("Scenes/Recovered.scene"));
}

TEST_CASE("Scene first save and SaveAs preserve paths history and files on failure", "[editor][e1]")
{
    using namespace Janus;
    ProjectTempDirectory directory;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    ProjectRuntimeConfig config;
    config.root = directory.Path();
    auto opened = Editor::ProjectSession::Open(config, *renderer);
    REQUIRE(opened);
    auto& session = *opened.Value();
    const auto initial = session.GetCurrentScenePath();
    REQUIRE(session.NewScene("Scenes/New.scene"));
    CHECK_FALSE(FileSystem::Exists(directory.Path() / "Scenes/New.scene"));
    REQUIRE(FileSystem::WriteText(directory.Path() / "Scenes/New.scene", "occupied"));
    CHECK_FALSE(session.SaveCurrentScene());
    CHECK(session.IsDirty());
    CHECK_FALSE(session.HasSavedSceneFile());
    CHECK(FileSystem::ReadText(directory.Path() / "Scenes/New.scene").Value() == "occupied");
    Editor::EditorContext context;
    context.project = &session;
    Editor::EditorActions actions(context);
    auto entity = actions.CreateEntity("Keep identity");
    REQUIRE(entity);
    const auto history = session.GetCommandBus().GetHistorySize();
    CHECK_FALSE(session.SaveSceneAs("../Outside.scene"));
    CHECK_FALSE(session.SaveSceneAs("Scenes/no-parent/File.scene"));
    CHECK_FALSE(session.SaveSceneAs(initial));
    CHECK(session.GetCurrentScenePath() == "Scenes/New.scene");
    REQUIRE(session.SaveSceneAs("Scenes/Saved.scene"));
    CHECK(session.HasSavedSceneFile());
    CHECK_FALSE(session.IsDirty());
    CHECK(session.GetCommandBus().GetHistorySize() == history);
    CHECK(session.GetEditorScene().FindEntity(entity.Value()).IsValid());
    CHECK(session.GetProjectSettings().defaultScene == initial);
    REQUIRE(actions.Undo());
    CHECK(session.IsDirty());
    REQUIRE(session.SaveCurrentScene());
    REQUIRE(session.OpenScene(initial));
    CHECK(session.GetCommandBus().GetHistorySize() == 0);
    REQUIRE(session.OpenScene("Scenes/Saved.scene"));
    CHECK(session.GetEditorScene().GetEntities().empty());
    REQUIRE(session.SaveSceneAs("Scenes/New.scene", true));
    CHECK(FileSystem::ReadText(directory.Path() / "Scenes/New.scene").Value() != "occupied");
}

TEST_CASE("Scene candidates reject stale context and changed disk without aborting transactions",
          "[editor][e1]")
{
    using namespace Janus;
    ProjectTempDirectory directory;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    ProjectRuntimeConfig config;
    config.root = directory.Path();
    auto opened = Editor::ProjectSession::Open(config, *renderer);
    REQUIRE(opened);
    auto& session = *opened.Value();
    const auto path = session.GetCurrentScenePath();
    auto candidate = session.PrepareOpenScene(path);
    REQUIRE(candidate);
    auto source = FileSystem::ReadText(directory.Path() / path);
    REQUIRE(source);
    REQUIRE(FileSystem::WriteText(directory.Path() / path, "malformed"));
    CHECK_FALSE(session.CommitPreparedScene(*candidate.Value()));
    CHECK(session.GetSceneRevision() == 0);
    auto invalidAssets = source.Value();
    // Keep the Scene codec valid, but replace one registered identity with an unknown asset.
    bool replaced = false;
    for (const auto& asset : session.GetAssetRegistry().GetAssets())
    {
        const auto id = asset.handle.ToString();
        const auto offset = invalidAssets.find(id);
        if (offset != std::string::npos)
        {
            invalidAssets.replace(offset, id.size(), UUID::Random().ToString());
            replaced = true;
            break;
        }
    }
    REQUIRE(replaced);
    REQUIRE(FileSystem::WriteText(directory.Path() / path, invalidAssets));
    CHECK_FALSE(session.PrepareOpenScene(path));
    REQUIRE(FileSystem::WriteText(directory.Path() / path, source.Value()));
    const auto owner = UUID::Random();
    auto transaction = session.BeginAuthoringTransaction(owner);
    REQUIRE(transaction);
    CHECK_FALSE(session.NewScene("Scenes/Blocked.scene"));
    CHECK_FALSE(session.SaveSceneAs("Scenes/Blocked.scene"));
    CHECK(session.GetCommandBus().GetTransactionId() == transaction.Value());
    REQUIRE(session.FinishAuthoringTransaction(transaction.Value(), owner, false));
    REQUIRE(session.PlayRuntime(true));
    CHECK_FALSE(session.CommitPreparedScene(*candidate.Value()));
    REQUIRE(session.StopRuntime());
    CHECK_FALSE(session.CommitPreparedScene(*candidate.Value()));
    candidate = session.PrepareOpenScene(path);
    REQUIRE(candidate);
    REQUIRE(session.CommitPreparedScene(*candidate.Value()));
    CHECK_FALSE(session.CommitPreparedScene(*candidate.Value()));
}

TEST_CASE("Human scene replacement requires explicit stop and preserves cancellation",
          "[editor][e1]")
{
    using namespace Janus;
    ProjectTempDirectory directory;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    ProjectRuntimeConfig config;
    config.root = directory.Path();
    auto opened = Editor::ProjectSession::Open(config, *renderer);
    REQUIRE(opened);
    auto& session = *opened.Value();
    const auto original = session.GetCurrentScenePath();
    REQUIRE(session.PlayRuntime(true));
    auto candidate = session.PrepareNewScene("Scenes/AfterStop.scene");
    REQUIRE(candidate);
    Editor::EditorCloseController leave(session);
    leave.Request();
    CHECK_FALSE(leave.ConfirmSceneChange(*candidate.Value(), false, false));
    CHECK(session.HasRuntime());
    CHECK(session.GetCurrentScenePath() == original);
    leave.Cancel();
    CHECK_FALSE(session.IsClosePending());
    leave.Request();
    REQUIRE(leave.ConfirmSceneChange(*candidate.Value(), false, true));
    CHECK_FALSE(session.HasRuntime());
    CHECK(session.GetCurrentScenePath() == "Scenes/AfterStop.scene");
    CHECK(session.IsDirty());
    CHECK_FALSE(leave.IsAccepted());
    CHECK_FALSE(leave.IsPending());
}

namespace
{
struct GenerationCommandState
{
    int value = 0;
    bool failExecute = false, failUndo = false, failRedo = false;
};

class GenerationCommand final : public Janus::ICommand
{
  public:
    explicit GenerationCommand(GenerationCommandState& state) : m_State(state) {}
    Janus::Result<void> Execute() override
    {
        if (m_State.failExecute)
            return Failed();
        ++m_State.value;
        return Janus::Result<void>::Success();
    }
    Janus::Result<void> Undo() override
    {
        if (m_State.failUndo)
            return Failed();
        --m_State.value;
        return Janus::Result<void>::Success();
    }
    Janus::Result<void> Redo() override
    {
        if (m_State.failRedo)
            return Failed();
        return Execute();
    }
    Janus::Result<Janus::usize> EstimateUndoBytes() const override
    {
        return Janus::Result<Janus::usize>::Success(64);
    }
    std::string_view Describe() const noexcept override
    {
        return "Generation test change";
    }

  private:
    static Janus::Result<void> Failed()
    {
        return Janus::Result<void>::Failure(Janus::ErrorCode::InvalidState,
                                            "Injected authoring failure");
    }
    GenerationCommandState& m_State;
};
} // namespace

TEST_CASE("Conditional authoring invalidates previews after execute undo redo and reload",
          "[editor][project-session][authoring-generation]")
{
    ProjectTempDirectory temp;
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open({temp.Path()}, *renderer);
    REQUIRE(opened);
    auto session = std::move(opened).Value();
    GenerationCommandState state;
    auto command = [&] { return std::make_unique<GenerationCommand>(state); };
    const auto revision = session->GetSceneRevision();
    auto generation = session->GetAuthoringGeneration();
    REQUIRE_FALSE(session->ExecuteAuthoringIfCurrent(command(), revision + 1, generation));
    REQUIRE_FALSE(session->ExecuteAuthoringIfCurrent(command(), revision, generation + 1));
    CHECK(state.value == 0);
    CHECK(session->GetCommandBus().GetHistorySize() == 0);
    CHECK_FALSE(session->IsDirty());
    REQUIRE(session->ExecuteAuthoringIfCurrent(command(), revision, generation));
    CHECK(session->GetAuthoringGeneration() == generation + 1);
    REQUIRE_FALSE(session->ExecuteAuthoringIfCurrent(command(), revision, generation));
    REQUIRE(session->UndoAuthoring());
    CHECK(state.value == 0);
    CHECK(session->GetAuthoringGeneration() == generation + 2);
    REQUIRE_FALSE(session->ExecuteAuthoringIfCurrent(command(), revision, generation));
    REQUIRE(session->RedoAuthoring());
    CHECK(session->GetAuthoringGeneration() == generation + 3);
    CHECK(session->GetSceneRevision() == revision);
    generation = session->GetAuthoringGeneration();
    REQUIRE(session->SaveCurrentScene());
    REQUIRE(session->SaveProjectSettings(session->GetProjectSettings()));
    CHECK(session->GetAuthoringGeneration() == generation);
    session->MarkDirty();
    CHECK(session->GetAuthoringGeneration() == generation + 1);
    generation = session->GetAuthoringGeneration();
    REQUIRE(session->DiscardUnsavedAndReload());
    CHECK(session->GetSceneRevision() == revision + 1);
    CHECK(session->GetAuthoringGeneration() == generation + 1);
    REQUIRE_FALSE(session->ExecuteAuthoringIfCurrent(command(), revision, generation));
    CHECK_FALSE(session->IsDirty());
    CHECK(session->GetCommandBus().GetHistorySize() == 0);
}

TEST_CASE("Conditional authoring rejects foreign threads and active runtime without executing",
          "[editor][project-session][authoring-generation]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open({SandboxProjectRoot()}, *renderer);
    REQUIRE(opened);
    auto session = std::move(opened).Value();
    GenerationCommandState state;
    const auto revision = session->GetSceneRevision();
    const auto generation = session->GetAuthoringGeneration();
    bool accepted = true;
    std::thread worker(
        [&]
        {
            accepted = static_cast<bool>(session->ExecuteAuthoringIfCurrent(
                std::make_unique<GenerationCommand>(state), revision, generation));
        });
    worker.join();
    CHECK_FALSE(accepted);
    REQUIRE(session->PlayRuntime(true));
    CHECK(session->GetAuthoringGeneration() == generation + 1);
    REQUIRE_FALSE(session->ExecuteAuthoringIfCurrent(std::make_unique<GenerationCommand>(state),
                                                     revision, generation));
    REQUIRE(session->StepRuntime());
    REQUIRE(session->StopRuntime());
    REQUIRE_FALSE(session->ExecuteAuthoringIfCurrent(std::make_unique<GenerationCommand>(state),
                                                     revision, generation));
    CHECK(state.value == 0);
    CHECK_FALSE(session->IsDirty());
    CHECK(session->GetAuthoringGeneration() == generation + 1);
    CHECK(session->GetCommandBus().GetHistorySize() == 0);
}

TEST_CASE("Authoring generation tracks agent edits and rollback but not commit grouping",
          "[editor][project-session][authoring-generation]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open({SandboxProjectRoot()}, *renderer);
    REQUIRE(opened);
    auto session = std::move(opened).Value();
    GenerationCommandState state;
    auto command = [&] { return std::make_unique<GenerationCommand>(state); };
    const auto owner = Janus::UUID::Random();
    const auto revision = session->GetSceneRevision();
    auto generation = session->GetAuthoringGeneration();
    auto token = session->BeginAuthoringTransaction(owner);
    REQUIRE(token);
    CHECK(session->GetAuthoringGeneration() == generation);
    REQUIRE(session->ExecuteAuthoring(command(), Janus::CommandActor::Agent, token.Value(), owner));
    CHECK(session->GetAuthoringGeneration() == ++generation);
    REQUIRE_FALSE(session->ExecuteAuthoringIfCurrent(command(), revision, generation));
    CHECK(session->GetCommandBus().HasTransaction());
    CHECK(session->GetCommandBus().GetPendingCount() == 1);
    CHECK(state.value == 1);
    CHECK(session->GetAuthoringGeneration() == generation);
    REQUIRE(session->FinishAuthoringTransaction(token.Value(), owner, true));
    CHECK(session->GetAuthoringGeneration() == generation);
    REQUIRE(session->FinishAuthoringTransaction(token.Value(), owner, true));
    CHECK(session->GetAuthoringGeneration() == generation);
    token = session->BeginAuthoringTransaction(owner);
    REQUIRE(token);
    REQUIRE(session->ExecuteAuthoring(command(), Janus::CommandActor::Agent, token.Value(), owner));
    CHECK(session->GetAuthoringGeneration() == ++generation);
    REQUIRE(session->FinishAuthoringTransaction(token.Value(), owner, false));
    CHECK(session->GetAuthoringGeneration() == ++generation);
    REQUIRE(session->FinishAuthoringTransaction(token.Value(), owner, false));
    CHECK(session->GetAuthoringGeneration() == generation);
    CHECK(state.value == 1);
    token = session->BeginAuthoringTransaction(owner);
    REQUIRE(token);
    REQUIRE(session->FinishAuthoringTransaction(token.Value(), owner, false));
    CHECK(session->GetAuthoringGeneration() == generation);
    CHECK(session->GetSceneRevision() == revision);
}

TEST_CASE("Authoring generation invalidates after automatic rollback and transaction expiry",
          "[editor][project-session][authoring-generation]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open({SandboxProjectRoot()}, *renderer);
    REQUIRE(opened);
    auto session = std::move(opened).Value();
    GenerationCommandState state, failure;
    failure.failExecute = true;
    const auto owner = Janus::UUID::Random();
    auto token = session->BeginAuthoringTransaction(owner);
    REQUIRE(token);
    REQUIRE(session->ExecuteAuthoring(std::make_unique<GenerationCommand>(state),
                                      Janus::CommandActor::Agent, token.Value(), owner));
    auto generation = session->GetAuthoringGeneration();
    REQUIRE_FALSE(session->ExecuteAuthoring(std::make_unique<GenerationCommand>(failure),
                                            Janus::CommandActor::Agent, token.Value(), owner));
    CHECK(session->GetAuthoringGeneration() == ++generation);
    CHECK(state.value == 0);
    CHECK_FALSE(session->IsDirty());
    CHECK_FALSE(session->GetCommandBus().HasTransaction());
    REQUIRE_FALSE(session->ExecuteAuthoring(std::make_unique<GenerationCommand>(failure)));
    CHECK(session->GetAuthoringGeneration() == generation);
    token = session->BeginAuthoringTransaction(owner);
    REQUIRE(token);
    REQUIRE(session->ExecuteAuthoring(std::make_unique<GenerationCommand>(state),
                                      Janus::CommandActor::Agent, token.Value(), owner));
    CHECK(session->GetAuthoringGeneration() == ++generation);
    session->ExpireAuthoringTransaction(std::chrono::steady_clock::now() +
                                        std::chrono::seconds(61));
    CHECK(session->GetAuthoringGeneration() == ++generation);
    CHECK(state.value == 0);
    CHECK_FALSE(session->IsDirty());
    CHECK_FALSE(session->GetCommandBus().HasTransaction());
}

TEST_CASE("Authoring generation invalidates failed transaction compensation and recovery",
          "[editor][project-session][authoring-generation]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open({SandboxProjectRoot()}, *renderer);
    REQUIRE(opened);
    auto session = std::move(opened).Value();
    GenerationCommandState state;
    state.failUndo = true;
    const auto owner = Janus::UUID::Random();
    auto token = session->BeginAuthoringTransaction(owner);
    REQUIRE(token);
    REQUIRE(session->ExecuteAuthoring(std::make_unique<GenerationCommand>(state),
                                      Janus::CommandActor::Agent, token.Value(), owner));
    auto generation = session->GetAuthoringGeneration();
    REQUIRE_FALSE(session->FinishAuthoringTransaction(token.Value(), owner, false));
    CHECK(session->GetCommandBus().RecoveryRequired());
    CHECK(session->GetAuthoringGeneration() == ++generation);
    REQUIRE_FALSE(session->ExecuteAuthoringIfCurrent(std::make_unique<GenerationCommand>(state),
                                                     session->GetSceneRevision(), generation));
    CHECK(session->GetAuthoringGeneration() == generation);
    REQUIRE(session->DiscardUnsavedAndReload());
    CHECK(session->GetAuthoringGeneration() == generation + 1);
    CHECK_FALSE(session->GetCommandBus().RecoveryRequired());
}

TEST_CASE("Failed grouped history compensation advances authoring generation",
          "[editor][project-session][authoring-generation]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open({SandboxProjectRoot()}, *renderer);
    REQUIRE(opened);
    auto session = std::move(opened).Value();
    GenerationCommandState first, second;
    const auto owner = Janus::UUID::Random();
    auto token = session->BeginAuthoringTransaction(owner);
    REQUIRE(token);
    REQUIRE(session->ExecuteAuthoring(std::make_unique<GenerationCommand>(first),
                                      Janus::CommandActor::Agent, token.Value(), owner));
    REQUIRE(session->ExecuteAuthoring(std::make_unique<GenerationCommand>(second),
                                      Janus::CommandActor::Agent, token.Value(), owner));
    REQUIRE(session->FinishAuthoringTransaction(token.Value(), owner, true));
    SECTION("Undo compensation")
    {
        first.failUndo = true;
        second.failRedo = true;
        const auto generation = session->GetAuthoringGeneration();
        REQUIRE_FALSE(session->UndoAuthoring());
        CHECK(session->GetAuthoringGeneration() == generation + 1);
    }
    SECTION("Redo compensation")
    {
        REQUIRE(session->UndoAuthoring());
        second.failRedo = true;
        first.failUndo = true;
        const auto generation = session->GetAuthoringGeneration();
        REQUIRE_FALSE(session->RedoAuthoring());
        CHECK(session->GetAuthoringGeneration() == generation + 1);
    }
    CHECK(session->GetCommandBus().RecoveryRequired());
}

TEST_CASE("Project settings save rebinds Lua after reopen and respects session guards",
          "[v0.10][project-settings]")
{
    ProjectTempDirectory temp;
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open({temp.Path()}, *renderer);
    REQUIRE(opened);
    auto session = std::move(opened).Value();
    auto settings = session->GetProjectSettings();
    settings.inputBindings["MoveLeft"] = {Janus::KeyCode::ArrowLeft};
    REQUIRE(Janus::FileSystem::WriteText(temp.Path() / "Scripts/PlayerController.lua", R"lua(
local Script = {}
function Script.OnUpdate(self, dt)
  local x,y = self.entity:get_position()
  if Input.is_action_down("MoveLeft") then self.entity:set_position(x - 10, y) end
end
return Script
)lua"));
    REQUIRE(session->SaveProjectSettings(settings));
    REQUIRE_FALSE(session->IsDirty());
    REQUIRE(session->GetCommandBus().GetHistorySize() == 0);
    const auto owner = Janus::UUID::Random();
    const auto transaction = session->BeginAuthoringTransaction(owner);
    REQUIRE(transaction);
    REQUIRE_FALSE(session->SaveProjectSettings(settings));
    REQUIRE(session->FinishAuthoringTransaction(transaction.Value(), owner, false));
    session.reset();
    session = std::move(Janus::Editor::ProjectSession::Open({temp.Path()}, *renderer)).Value();
    REQUIRE(session->GetProjectSettings() == settings);
    Janus::UUID player;
    float initialX = 0;
    for (auto entity : session->GetEditorScene().GetEntities())
    {
        const auto* identity =
            session->GetEditorScene().GetComponent<Janus::EntityIdentityComponent>(entity);
        if (identity->name == "Player")
        {
            player = identity->id;
            initialX = session->GetEditorScene()
                           .GetComponent<Janus::TransformComponent>(entity)
                           ->position.x;
        }
    }
    REQUIRE(player.IsValid());
    Janus::InputState input;
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::A, false});
    REQUIRE(session->StartRuntime(input, true));
    REQUIRE_FALSE(session->SaveProjectSettings(settings));
    REQUIRE(session->StepRuntime());
    auto& runtime = session->GetRuntimeSession()->GetScene();
    auto* transform = runtime.GetComponent<Janus::TransformComponent>(runtime.FindEntity(player));
    REQUIRE(transform->position.x == initialX);
    REQUIRE(session->ResumeRuntime());
    REQUIRE(session->UpdateRuntime(Janus::TimeStep::FromSeconds(1.0 / 60)));
    REQUIRE(transform->position.x == initialX);
    input.BeginFrame();
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::ArrowLeft, false});
    REQUIRE(session->UpdateRuntime(Janus::TimeStep::FromSeconds(1.0 / 60)));
    REQUIRE(transform->position.x == initialX - 10);
    REQUIRE(session->StopRuntime());
    REQUIRE_FALSE(session->IsDirty());
    REQUIRE(
        session->GetEditorScene()
            .GetComponent<Janus::TransformComponent>(session->GetEditorScene().FindEntity(player))
            ->position.x == initialX);
    const auto saved = session->GetProjectSettings();
    settings.targetFps = 1001;
    REQUIRE_FALSE(session->SaveProjectSettings(settings));
    REQUIRE(session->GetProjectSettings() == saved);
}

TEST_CASE("Project settings disk failure preserves session state", "[v0.10][project-settings]")
{
    ProjectTempDirectory temp;
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open({temp.Path()}, *renderer);
    REQUIRE(opened);
    auto session = std::move(opened).Value();
    const auto original = session->GetProjectSettings();
    auto changed = original;
    changed.name = "Unsaved";
    std::error_code error;
    std::filesystem::remove(temp.Path() / "project.json", error);
    REQUIRE_FALSE(error);
    REQUIRE(std::filesystem::create_directory(temp.Path() / "project.json", error));
    REQUIRE_FALSE(error);
    REQUIRE_FALSE(session->SaveProjectSettings(changed));
    REQUIRE(session->GetProjectSettings() == original);
    REQUIRE_FALSE(session->IsDirty());
}

TEST_CASE("Legacy projects without a manifest retain Lua keyboard gameplay",
          "[v0.10][project-settings]")
{
    ProjectTempDirectory temp;
    std::error_code error;
    std::filesystem::remove(temp.Path() / "project.json", error);
    REQUIRE_FALSE(error);
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto opened = Janus::Editor::ProjectSession::Open({temp.Path()}, *renderer);
    REQUIRE(opened);
    auto session = std::move(opened).Value();
    REQUIRE(session->GetProjectSettings().inputBindings.empty());
    Janus::InputState input;
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::D, false});
    REQUIRE(session->StartRuntime(input));
    REQUIRE(session->UpdateRuntime(Janus::TimeStep::FromSeconds(1.0 / 60)));
    auto& runtime = session->GetRuntimeSession()->GetScene();
    bool moved = false;
    for (const auto entity : runtime.GetEntities())
    {
        if (runtime.GetComponent<Janus::EntityIdentityComponent>(entity)->name == "Player")
            moved = runtime.GetComponent<Janus::TransformComponent>(entity)->position.x > -80;
    }
    REQUIRE(moved);
    REQUIRE_FALSE(Janus::FileSystem::Exists(temp.Path() / "project.json"));
}

TEST_CASE(
    "ProjectSession opens the disk-backed SandboxProject for authoring",
    "[editor][project-session][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::ProjectRuntimeConfig config;
    config.root = SandboxProjectRoot();

    auto result = Janus::Editor::ProjectSession::Open(
        config,
        *renderer);

    REQUIRE(result);

    auto session = std::move(result).Value();
    REQUIRE(session->GetProjectRoot() == config.root);
    REQUIRE(
        session->GetCurrentScenePath()
        == std::filesystem::path("Scenes/Battle.scene"));
    REQUIRE(session->GetAssetRegistry().Size() == 6);
    REQUIRE(session->GetReflectionRegistry().GetComponentCount() == 14);
    REQUIRE(
        session->GetReflectionRegistry().FindComponent(Janus::MakeComponentTypeId("AudioSource")));
    REQUIRE(session->GetCommandBus().GetHistorySize() == 0);
    REQUIRE(session->GetCommandBus().GetCursor() == 0);
    REQUIRE(session->GetEditorScene().GetMetadata().name == "Battle");
    REQUIRE(session->GetEditorScene().GetEntities().size() == 4);
}

TEST_CASE(
    "ProjectSession rejects project-relative path escapes",
    "[editor][project-session][v0.6][errors]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::ProjectRuntimeConfig config;
    config.root = SandboxProjectRoot();
    config.assetRegistryPath = "../AssetRegistry.json";

    const auto result = Janus::Editor::ProjectSession::Open(
        config,
        *renderer);

    REQUIRE_FALSE(result);
    REQUIRE(
        result.GetError().code
        == Janus::ErrorCode::InvalidArgument);
}

TEST_CASE(
    "ProjectSession surfaces missing project files",
    "[editor][project-session][v0.6][errors]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::ProjectRuntimeConfig config;
    config.root =
        SandboxProjectRoot().parent_path() / "MissingJanusProject";

    const auto result = Janus::Editor::ProjectSession::Open(
        config,
        *renderer);

    REQUIRE_FALSE(result);
}


TEST_CASE(
    "ProjectSession saves dirty authoring Scene and clears dirty state",
    "[editor][project-session][save][v0.6]")
{
    ProjectTempDirectory temp;

    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::ProjectRuntimeConfig config;
    config.root = temp.Path();

    auto opened =
        Janus::Editor::ProjectSession::Open(
            config,
            *renderer);
    REQUIRE(opened);

    auto session = std::move(opened).Value();
    REQUIRE_FALSE(session->IsDirty());

    session->GetEditorScene().SetName("Saved Battle");
    session->MarkDirty();
    REQUIRE(session->IsDirty());

    REQUIRE(session->SaveCurrentScene());
    REQUIRE_FALSE(session->IsDirty());

    auto loaded =
        Janus::SceneDeserializer::Load(
            temp.Path() / "Scenes/Battle.scene",
            session->GetReflectionRegistry());
    REQUIRE(loaded);
    REQUIRE(
        loaded.Value()->GetMetadata().name
        == "Saved Battle");
}

TEST_CASE(
    "ProjectSession preserves dirty state when Scene save fails",
    "[editor][project-session][save][v0.6][errors]")
{
    ProjectTempDirectory temp;

    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::ProjectRuntimeConfig config;
    config.root = temp.Path();

    auto opened =
        Janus::Editor::ProjectSession::Open(
            config,
            *renderer);
    REQUIRE(opened);

    auto session = std::move(opened).Value();
    session->MarkDirty();

    std::error_code error;
    std::filesystem::remove_all(
        temp.Path() / "Scenes",
        error);
    REQUIRE_FALSE(error);

    REQUIRE(
        Janus::FileSystem::WriteText(
            temp.Path() / "Scenes",
            "not-a-directory"));

    const auto saved =
        session->SaveCurrentScene();

    REQUIRE_FALSE(saved);
    REQUIRE(session->IsDirty());
}

TEST_CASE(
    "RuntimeSession changes never dirty the authoring Scene",
    "[editor][project-session][dirty][runtime][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::ProjectRuntimeConfig config;
    config.root = SandboxProjectRoot();

    auto opened =
        Janus::Editor::ProjectSession::Open(
            config,
            *renderer);
    REQUIRE(opened);

    auto session = std::move(opened).Value();
    REQUIRE_FALSE(session->IsDirty());

    Janus::InputState input;
    input.BeginFrame();
    input.Apply(
        Janus::KeyPressedEvent{
            Janus::KeyCode::D,
            false});

    REQUIRE(session->StartRuntime(input));
    REQUIRE(
        session->UpdateRuntime(
            Janus::TimeStep::FromSeconds(0.25)));
    REQUIRE_FALSE(session->IsDirty());

    session->MarkDirty();
    REQUIRE(session->IsDirty());

    const auto saveDuringPlay =
        session->SaveCurrentScene();
    REQUIRE_FALSE(saveDuringPlay);
    REQUIRE(
        saveDuringPlay.GetError().code
        == Janus::ErrorCode::InvalidState);
    REQUIRE(session->IsDirty());

    REQUIRE(session->StopRuntime());
    REQUIRE(session->IsDirty());
}
