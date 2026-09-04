#include "ProjectSession.h"
#include "RuntimeSession.h"

#include "Application/ApplicationConfig.h"
#include "Asset/AssetHandle.h"
#include "Core/Event/Event.h"
#include "Core/Input/InputState.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string_view>
#include <utility>

namespace
{

std::filesystem::path SandboxProjectRoot()
{
    return std::filesystem::path(JANUS_TEST_SOURCE_DIR)
        .parent_path()
        / "SandboxProject";
}

Janus::ECS::Entity FindByName(
    Janus::Scene& scene,
    std::string_view name)
{
    for (const Janus::ECS::Entity entity : scene.GetEntities())
    {
        const auto* identity =
            scene.GetComponent<Janus::EntityIdentityComponent>(entity);
        if (identity != nullptr && identity->name == name)
        {
            return entity;
        }
    }
    return {};
}

std::unique_ptr<Janus::Editor::ProjectSession> OpenProject(
    Janus::Renderer2D& renderer)
{
    Janus::ProjectRuntimeConfig config;
    config.root = SandboxProjectRoot();

    auto opened = Janus::Editor::ProjectSession::Open(
        config,
        renderer);
    REQUIRE(opened);
    return std::move(opened).Value();
}

} // namespace

TEST_CASE(
    "ProjectSession runs Lua only against an isolated RuntimeScene",
    "[editor][runtime-session][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    auto& editorScene = project->GetEditorScene();
    const auto editorPlayer = FindByName(editorScene, "Player");
    REQUIRE(editorPlayer.IsValid());

    const auto* editorIdentity =
        editorScene.GetComponent<Janus::EntityIdentityComponent>(
            editorPlayer);
    auto* editorTransform =
        editorScene.GetComponent<Janus::TransformComponent>(
            editorPlayer);
    REQUIRE(editorIdentity != nullptr);
    REQUIRE(editorTransform != nullptr);
    REQUIRE(editorTransform->position.x == Catch::Approx(-80.0f));

    Janus::InputState input;
    input.BeginFrame();
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::D, false});

    REQUIRE(project->StartRuntime(input));
    REQUIRE(project->IsPlaying());
    REQUIRE(project->GetRuntimeSession() != nullptr);

    auto& runtimeScene = project->GetRuntimeSession()->GetScene();
    const auto runtimePlayer =
        runtimeScene.FindEntity(editorIdentity->id);
    REQUIRE(runtimePlayer.IsValid());

    auto* runtimeTransform =
        runtimeScene.GetComponent<Janus::TransformComponent>(
            runtimePlayer);
    REQUIRE(runtimeTransform != nullptr);
    REQUIRE(runtimeTransform->position.x == Catch::Approx(-80.0f));

    REQUIRE(project->UpdateRuntime(
        Janus::TimeStep::FromSeconds(1.0)));

    REQUIRE(runtimeTransform->position.x == Catch::Approx(100.0f));
    REQUIRE(editorTransform->position.x == Catch::Approx(-80.0f));

    REQUIRE(project->StopRuntime());
    REQUIRE_FALSE(project->IsPlaying());
    REQUIRE(project->GetRuntimeSession() == nullptr);
    REQUIRE(editorTransform->position.x == Catch::Approx(-80.0f));

    // A new Play starts from authoring state again, not the previous runtime.
    REQUIRE(project->StartRuntime(input));
    auto& secondRuntime = project->GetRuntimeSession()->GetScene();
    const auto secondPlayer =
        secondRuntime.FindEntity(editorIdentity->id);
    const auto* secondTransform =
        secondRuntime.GetComponent<Janus::TransformComponent>(
            secondPlayer);
    REQUIRE(secondTransform != nullptr);
    REQUIRE(secondTransform->position.x == Catch::Approx(-80.0f));
    REQUIRE(project->StopRuntime());
}

TEST_CASE(
    "ProjectSession rejects duplicate Play and cleans failed runtime startup",
    "[editor][runtime-session][v0.6][errors]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    Janus::InputState input;

    REQUIRE(project->StartRuntime(input));

    const auto duplicate = project->StartRuntime(input);
    REQUIRE_FALSE(duplicate);
    REQUIRE(duplicate.GetError().code == Janus::ErrorCode::InvalidState);
    REQUIRE(project->IsPlaying());

    REQUIRE(project->StopRuntime());
    REQUIRE_FALSE(project->IsPlaying());

    const auto updateWithoutRuntime =
        project->UpdateRuntime(Janus::TimeStep::FromSeconds(0.016));
    REQUIRE_FALSE(updateWithoutRuntime);
    REQUIRE(
        updateWithoutRuntime.GetError().code
        == Janus::ErrorCode::InvalidState);

    auto& editorScene = project->GetEditorScene();
    const auto player = FindByName(editorScene, "Player");
    REQUIRE(player.IsValid());

    auto* script =
        editorScene.GetComponent<Janus::LuaScriptComponent>(player);
    REQUIRE(script != nullptr);
    script->script = Janus::AssetHandle::Random();

    const auto failedStart = project->StartRuntime(input);
    REQUIRE_FALSE(failedStart);
    REQUIRE(
        failedStart.GetError().code
        == Janus::ErrorCode::AssetNotFound);
    REQUIRE_FALSE(project->IsPlaying());
    REQUIRE(project->GetRuntimeSession() == nullptr);
}
