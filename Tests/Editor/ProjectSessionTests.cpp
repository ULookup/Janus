#include "ProjectSession.h"

#include "Application/ApplicationConfig.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Input/InputState.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

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
    REQUIRE(session->GetAssetRegistry().Size() == 2);
    REQUIRE(session->GetReflectionRegistry().GetComponentCount() == 4);
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
