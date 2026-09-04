#include "ProjectSession.h"

#include "Application/ApplicationConfig.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <utility>

namespace
{

std::filesystem::path SandboxProjectRoot()
{
    return std::filesystem::path(JANUS_TEST_SOURCE_DIR)
        .parent_path()
        / "SandboxProject";
}

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
