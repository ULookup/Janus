#include "EditorActions.h"
#include "EditorContext.h"
#include "ProjectSession.h"
#include "RuntimeSession.h"

#include "Application/ApplicationConfig.h"
#include "Asset/AssetMetadata.h"
#include "Core/Input/InputState.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneRenderer.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_approx.hpp>
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

class EditorWorkflowTempProject final
{
public:
    EditorWorkflowTempProject()
    {
        const auto leaf =
            "janus-editor-workflow-"
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
                "Failed to copy SandboxProject for Editor workflow test.");
        }
    }

    ~EditorWorkflowTempProject()
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
    "v0.6 authoring workflow saves plays renders and discards runtime state",
    "[editor][workflow][v0.6][e2e]")
{
    EditorWorkflowTempProject temp;

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

    auto project = std::move(opened).Value();

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    const auto authored =
        actions.CreateEntity("EditorAuthored");
    REQUIRE(authored);

    REQUIRE(
        actions.SetTransform(
            authored.Value(),
            Janus::Vector2{-40.0f, 20.0f},
            0.0f,
            Janus::Vector2{1.0f, 1.0f}));
    REQUIRE(actions.AddSpriteRenderer(authored.Value()));
    REQUIRE(actions.AddLuaScript(authored.Value()));

    Janus::AssetHandle texture;
    Janus::AssetHandle script;

    for (const Janus::AssetMetadata& asset :
         project->GetAssetRegistry().GetAssets())
    {
        if (asset.type == Janus::AssetType::Texture)
        {
            texture = asset.handle;
        }
        else if (asset.type == Janus::AssetType::LuaScript)
        {
            script = asset.handle;
        }
    }

    REQUIRE(texture.IsValid());
    REQUIRE(script.IsValid());

    REQUIRE(
        actions.SetSpriteTexture(
            authored.Value(),
            texture));
    REQUIRE(
        actions.SetLuaScriptAsset(
            authored.Value(),
            script));
    REQUIRE(
        actions.SetSpriteRenderer(
            authored.Value(),
            Janus::Vector2{32.0f, 32.0f},
            Janus::Color::White(),
            3,
            true));
    REQUIRE(
        actions.SetLuaScriptEnabled(
            authored.Value(),
            true));

    REQUIRE(project->IsDirty());
    REQUIRE(project->SaveCurrentScene());
    REQUIRE_FALSE(project->IsDirty());

    const auto editorEntity =
        project->GetEditorScene().FindEntity(
            authored.Value());
    REQUIRE(editorEntity.IsValid());

    const auto* editorTransform =
        project->GetEditorScene()
            .GetComponent<Janus::TransformComponent>(
                editorEntity);
    REQUIRE(editorTransform != nullptr);
    REQUIRE(
        editorTransform->position.x
        == Catch::Approx(-40.0f));

    Janus::InputState input;
    input.BeginFrame();
    input.Apply(
        Janus::KeyPressedEvent{
            Janus::KeyCode::D,
            false});

    REQUIRE(project->StartRuntime(input));
    REQUIRE(project->IsPlaying());

    REQUIRE(
        project->UpdateRuntime(
            Janus::TimeStep::FromSeconds(1.0)));

    auto* runtime =
        project->GetRuntimeSession();
    REQUIRE(runtime != nullptr);

    Janus::Scene& runtimeScene =
        runtime->GetScene();

    const auto runtimeEntity =
        runtimeScene.FindEntity(
            authored.Value());
    REQUIRE(runtimeEntity.IsValid());

    const auto* runtimeTransform =
        runtimeScene.GetComponent<Janus::TransformComponent>(
            runtimeEntity);
    REQUIRE(runtimeTransform != nullptr);
    REQUIRE(
        runtimeTransform->position.x
        == Catch::Approx(140.0f));

    // Gameplay only mutates RuntimeScene.
    REQUIRE(
        editorTransform->position.x
        == Catch::Approx(-40.0f));
    REQUIRE_FALSE(project->IsDirty());

    Janus::SceneRenderer sceneRenderer;
    const auto gameCamera =
        sceneRenderer.ResolvePrimaryCamera(
            runtimeScene);
    REQUIRE(gameCamera);

    const auto rendered =
        sceneRenderer.Render(
            Janus::SceneRenderRequest{
                runtimeScene,
                project->GetAssetService(),
                *renderer,
                gameCamera.Value(),
                Janus::Viewport{800, 600},
                {}});
    REQUIRE(rendered);
    REQUIRE_FALSE(device.drawCommands.empty());

    REQUIRE(project->StopRuntime());
    REQUIRE_FALSE(project->IsPlaying());
    REQUIRE(project->GetRuntimeSession() == nullptr);

    const auto editorAfterStop =
        project->GetEditorScene().FindEntity(
            authored.Value());
    REQUIRE(editorAfterStop.IsValid());

    const auto* transformAfterStop =
        project->GetEditorScene()
            .GetComponent<Janus::TransformComponent>(
                editorAfterStop);
    REQUIRE(transformAfterStop != nullptr);
    REQUIRE(
        transformAfterStop->position.x
        == Catch::Approx(-40.0f));

    auto reloaded =
        Janus::SceneDeserializer::Load(
            temp.Path() / "Scenes/Battle.scene");
    REQUIRE(reloaded);

    const auto persisted =
        reloaded.Value()->FindEntity(
            authored.Value());
    REQUIRE(persisted.IsValid());

    const auto* persistedTransform =
        reloaded.Value()
            ->GetComponent<Janus::TransformComponent>(
                persisted);
    const auto* persistedSprite =
        reloaded.Value()
            ->GetComponent<Janus::SpriteRendererComponent>(
                persisted);
    const auto* persistedScript =
        reloaded.Value()
            ->GetComponent<Janus::LuaScriptComponent>(
                persisted);

    REQUIRE(persistedTransform != nullptr);
    REQUIRE(persistedSprite != nullptr);
    REQUIRE(persistedScript != nullptr);
    REQUIRE(
        persistedTransform->position.x
        == Catch::Approx(-40.0f));
    REQUIRE(persistedSprite->texture == texture);
    REQUIRE(persistedScript->script == script);
    REQUIRE(persistedScript->enabled);
}
