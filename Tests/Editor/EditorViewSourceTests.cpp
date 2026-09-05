#include "EditorViewSource.h"

#include "ProjectSession.h"

#include "Application/ApplicationConfig.h"
#include "Core/Input/InputState.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
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
    "Editor view source policy keeps Scene View authoring-only",
    "[editor][view-source][v0.6]")
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

    auto project = std::move(opened).Value();
    auto* editorScene = &project->GetEditorScene();

    REQUIRE(
        &Janus::Editor::ResolveSceneViewScene(*project)
        == editorScene);
    REQUIRE(
        &Janus::Editor::ResolveGameViewScene(*project)
        == editorScene);

    Janus::InputState input;
    REQUIRE(project->StartRuntime(input));

    auto* runtimeScene =
        &project->GetRuntimeSession()->GetScene();
    REQUIRE(runtimeScene != editorScene);

    REQUIRE(
        &Janus::Editor::ResolveSceneViewScene(*project)
        == editorScene);
    REQUIRE(
        &Janus::Editor::ResolveGameViewScene(*project)
        == runtimeScene);

    REQUIRE(project->StopRuntime());

    REQUIRE(
        &Janus::Editor::ResolveSceneViewScene(*project)
        == editorScene);
    REQUIRE(
        &Janus::Editor::ResolveGameViewScene(*project)
        == editorScene);
}
