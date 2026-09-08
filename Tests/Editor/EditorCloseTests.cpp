#include "../Renderer/FakeRenderDevice.h"
#include "Core/FileSystem/FileSystem.h"
#include "EditorCloseController.h"
#include "ProjectSession.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Command/EntityCommands.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace
{
struct CloseFixture
{
    std::filesystem::path root = std::filesystem::temp_directory_path() /
                                 ("janus-close-" + Janus::UUID::Random().ToString());
    Janus::Test::FakeRenderDevice device;
    std::unique_ptr<Janus::Renderer2D> renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    std::unique_ptr<Janus::Editor::ProjectSession> session;
    CloseFixture()
    {
        std::filesystem::copy(std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() /
                                  "SandboxProject",
                              root, std::filesystem::copy_options::recursive);
        auto result = Janus::Editor::ProjectSession::Open({root}, *renderer);
        REQUIRE(result);
        session = std::move(result).Value();
    }
    ~CloseFixture()
    {
        session.reset();
        renderer.reset();
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
    Janus::UUID Edit()
    {
        auto id = Janus::UUID::Random();
        REQUIRE(session->ExecuteAuthoring(std::make_unique<Janus::CreateEntityCommand>(
            session->GetEditorScene(), id, "Close probe")));
        return id;
    }
};
} // namespace

TEST_CASE("Close cancellation preserves authoring and releases the session write guard",
          "[editor][close]")
{
    CloseFixture f;
    const auto id = f.Edit();
    Janus::Editor::EditorCloseController close(*f.session);
    close.Request();
    close.Request();
    REQUIRE(close.IsPending());
    CHECK(f.session->IsClosePending());
    CHECK_FALSE(f.session->SaveCurrentScene());
    CHECK_FALSE(f.session->UndoAuthoring());
    CHECK_FALSE(f.session->PlayRuntime());
    CHECK_FALSE(f.session->SaveProjectSettings(f.session->GetProjectSettings()));
    CHECK_FALSE(f.session->BeginAuthoringTransaction(Janus::UUID::Random()));
    CHECK_FALSE(f.session->DiscardUnsavedAndReload());
    close.Cancel();
    CHECK_FALSE(f.session->IsClosePending());
    CHECK(f.session->IsDirty());
    CHECK(f.session->GetEditorScene().FindEntity(id).IsValid());
    REQUIRE(f.session->UndoAuthoring());
}

TEST_CASE("Close saves authoring before acceptance and discard never writes the scene",
          "[editor][close]")
{
    CloseFixture f;
    const auto path = f.root / f.session->GetCurrentScenePath();
    const auto before = Janus::FileSystem::ReadText(path);
    REQUIRE(before);
    const auto id = f.Edit();
    Janus::Editor::EditorCloseController close(*f.session);
    close.Request();
    const bool save = GENERATE(true, false);
    REQUIRE(close.Confirm(save, false, nullptr));
    CHECK(close.IsAccepted());
    CHECK(f.session->IsClosePending());
    if (save)
    {
        auto loaded = Janus::SceneDeserializer::Load(path, f.session->GetReflectionRegistry());
        REQUIRE(loaded);
        CHECK(loaded.Value()->FindEntity(id).IsValid());
        CHECK_FALSE(f.session->IsDirty());
    }
    else
    {
        CHECK(Janus::FileSystem::ReadText(path).Value() == before.Value());
        CHECK(f.session->IsDirty());
    }
}

TEST_CASE("Close save failure keeps dirty scene and can retry without losing history",
          "[editor][close]")
{
    CloseFixture f;
    f.Edit();
    auto path = f.root / f.session->GetCurrentScenePath();
    auto backup = path;
    backup += ".backup";
    std::filesystem::rename(path, backup);
    std::filesystem::create_directory(path); // Atomic replacement of a directory must fail.
    Janus::Editor::EditorCloseController close(*f.session);
    close.Request();
    CHECK_FALSE(close.Confirm(true, false, nullptr));
    CHECK(close.IsPending());
    CHECK_FALSE(close.IsAccepted());
    CHECK(f.session->IsDirty());
    CHECK(f.session->GetCommandBus().GetHistorySize() == 1);
    std::filesystem::remove(path);
    std::filesystem::rename(backup, path);
    REQUIRE(close.Confirm(true, false, nullptr));
    CHECK(close.IsAccepted());
}

TEST_CASE("Close requires explicit transaction rollback and reevaluates restored dirty state",
          "[editor][close]")
{
    CloseFixture f;
    const auto owner = Janus::UUID::Random();
    auto transaction = f.session->BeginAuthoringTransaction(owner);
    REQUIRE(transaction);
    const auto id = Janus::UUID::Random();
    REQUIRE(f.session->ExecuteAuthoring(
        std::make_unique<Janus::CreateEntityCommand>(f.session->GetEditorScene(), id, "Pending"),
        Janus::CommandActor::Agent, transaction.Value(), owner));
    Janus::Editor::EditorCloseController close(*f.session);
    close.Request();
    CHECK_FALSE(close.Confirm(true, false, nullptr));
    CHECK(f.session->GetCommandBus().HasTransaction());
    REQUIRE(close.RollbackTransaction());
    CHECK_FALSE(f.session->GetEditorScene().FindEntity(id).IsValid());
    CHECK_FALSE(f.session->IsDirty());
    REQUIRE(close.Confirm(true, false, nullptr));
}

TEST_CASE("Close handles paused runtime and validates settings before saving scene",
          "[editor][close]")
{
    CloseFixture f;
    REQUIRE(f.session->PlayRuntime(true));
    Janus::Editor::EditorCloseController close(*f.session);
    close.Request();
    CHECK_FALSE(close.Confirm(true, false, nullptr));
    CHECK(f.session->HasRuntime());
    auto settings = f.session->GetProjectSettings();
    settings.width = 0;
    CHECK_FALSE(close.Confirm(true, true, &settings));
    CHECK(f.session->HasRuntime());
    settings.width = 800;
    REQUIRE(close.Confirm(true, true, &settings));
    CHECK_FALSE(f.session->HasRuntime());
    CHECK(f.session->GetProjectSettings().width == 800);
    CHECK(close.IsAccepted());
}

TEST_CASE("Close refuses to save failed compensation and requires explicit discard",
          "[editor][close]")
{
    struct BrokenUndo final : Janus::ICommand
    {
        Janus::Result<void> Execute() override
        {
            return Janus::Result<void>::Success();
        }
        Janus::Result<void> Undo() override
        {
            return Janus::Result<void>::Failure(Janus::ErrorCode::InvalidState,
                                                "Injected compensation failure");
        }
        Janus::Result<void> Redo() override
        {
            return Execute();
        }
        Janus::Result<Janus::usize> EstimateUndoBytes() const override
        {
            return Janus::Result<Janus::usize>::Success(64);
        }
        std::string_view Describe() const noexcept override
        {
            return "Broken undo";
        }
    };
    CloseFixture f;
    const auto owner = Janus::UUID::Random();
    auto tx = f.session->BeginAuthoringTransaction(owner);
    REQUIRE(tx);
    REQUIRE(f.session->ExecuteAuthoring(std::make_unique<BrokenUndo>(), Janus::CommandActor::Agent,
                                        tx.Value(), owner));
    Janus::Editor::EditorCloseController close(*f.session);
    close.Request();
    CHECK_FALSE(close.RollbackTransaction());
    CHECK(f.session->GetCommandBus().RecoveryRequired());
    CHECK_FALSE(close.Confirm(true, false, nullptr));
    CHECK_FALSE(close.IsAccepted());
    REQUIRE(close.Confirm(false, false, nullptr));
    CHECK(close.IsAccepted());
    CHECK(f.session->GetCommandBus().RecoveryRequired());
}

TEST_CASE("Close retains a failed settings draft and a faulted runtime until confirmed",
          "[editor][close]")
{
    CloseFixture f;
    REQUIRE(Janus::FileSystem::WriteText(f.root / "Scripts/PlayerController.lua", R"lua(
local S = {}
function S.OnUpdate(self, dt) error('close fault probe') end
return S
)lua"));
    REQUIRE(f.session->PlayRuntime());
    REQUIRE_FALSE(f.session->UpdateRuntime(Janus::TimeStep::FromSeconds(1.0f / 60.0f)));
    REQUIRE(f.session->GetRuntimeState() == Janus::RuntimeState::Faulted);
    Janus::Editor::EditorCloseController close(*f.session);
    close.Request();
    close.Cancel();
    CHECK(f.session->GetRuntimeState() == Janus::RuntimeState::Faulted);
    close.Request();
    const auto original = f.session->GetProjectSettings();
    auto draft = original;
    draft.name = "Close draft";
    // The fixture has no manifest by default; a directory prevents the atomic file write.
    const auto manifest = f.root / "project.json";
    std::error_code error;
    std::filesystem::remove(manifest, error);
    std::filesystem::create_directory(manifest);
    CHECK_FALSE(close.Confirm(true, true, &draft));
    CHECK_FALSE(close.IsAccepted());
    CHECK(f.session->GetProjectSettings() == original);
    CHECK(draft.name == "Close draft");
    CHECK_FALSE(f.session->HasRuntime()); // An already confirmed Stop is not undone on IO failure.
    std::filesystem::remove(manifest);
    REQUIRE(close.Confirm(true, true, &draft));
    CHECK(f.session->GetProjectSettings() == draft);
}
