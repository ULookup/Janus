#include "EditorActions.h"
#include "EditorContext.h"
#include "ProjectSession.h"

#include "Application/ApplicationConfig.h"
#include "Core/Input/InputState.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
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

std::unique_ptr<Janus::Editor::ProjectSession> OpenProject(
    Janus::Renderer2D& renderer)
{
    Janus::ProjectRuntimeConfig config;
    config.root = SandboxProjectRoot();

    auto opened =
        Janus::Editor::ProjectSession::Open(
            config,
            renderer);
    REQUIRE(opened);
    return std::move(opened).Value();
}

class ProjectTempDirectory final
{
public:
    ProjectTempDirectory()
    {
        const auto leaf =
            "janus-editor-actions-"
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
                "Failed to copy SandboxProject for EditorActions tests.");
        }
    }

    ~ProjectTempDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(
            m_Path,
            error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return m_Path;
    }

private:
    std::filesystem::path m_Path;
};

std::unique_ptr<Janus::Editor::ProjectSession> OpenTempProject(
    Janus::Renderer2D& renderer,
    const std::filesystem::path& root)
{
    Janus::ProjectRuntimeConfig config;
    config.root = root;

    auto opened =
        Janus::Editor::ProjectSession::Open(
            config,
            renderer);
    REQUIRE(opened);
    return std::move(opened).Value();
}

} // namespace

TEST_CASE(
    "EditorActions create rename transform and delete authoring entities",
    "[editor][actions][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    const auto created = actions.CreateEntity("Draft");
    REQUIRE(created);
    REQUIRE(context.selection.HasSelection());

    auto& scene = project->GetEditorScene();
    const auto entity = scene.FindEntity(created.Value());
    REQUIRE(entity.IsValid());

    REQUIRE(actions.RenameEntity(created.Value(), "Renamed"));
    REQUIRE(
        scene.GetComponent<Janus::EntityIdentityComponent>(entity)->name
        == "Renamed");

    REQUIRE(
        actions.SetTransform(
            created.Value(),
            Janus::Vector2{10.0f, 20.0f},
            0.5f,
            Janus::Vector2{2.0f, 3.0f}));

    const auto* transform =
        scene.GetComponent<Janus::TransformComponent>(entity);
    REQUIRE(transform != nullptr);
    REQUIRE(transform->position.x == Catch::Approx(10.0f));
    REQUIRE(transform->position.y == Catch::Approx(20.0f));
    REQUIRE(transform->rotationRadians == Catch::Approx(0.5f));
    REQUIRE(transform->scale.x == Catch::Approx(2.0f));
    REQUIRE(transform->scale.y == Catch::Approx(3.0f));
    REQUIRE(transform->dirty);

    REQUIRE(actions.DeleteEntity(created.Value()));
    REQUIRE_FALSE(scene.FindEntity(created.Value()).IsValid());
    REQUIRE_FALSE(context.selection.HasSelection());
}

TEST_CASE(
    "EditorActions add and remove supported optional components",
    "[editor][actions][components][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    const auto created = actions.CreateEntity("Components");
    REQUIRE(created);

    auto& scene = project->GetEditorScene();
    const auto entity = scene.FindEntity(created.Value());

    REQUIRE(actions.AddSpriteRenderer(created.Value()));
    REQUIRE(scene.HasComponent<Janus::SpriteRendererComponent>(entity));
    REQUIRE(
        actions.SetSpriteRenderer(
            created.Value(),
            Janus::Vector2{32.0f, 48.0f},
            Janus::Color{0.5f, 0.6f, 0.7f, 0.8f},
            4,
            true));

    const auto* sprite =
        scene.GetComponent<Janus::SpriteRendererComponent>(entity);
    REQUIRE(sprite != nullptr);
    REQUIRE(sprite->size.x == Catch::Approx(32.0f));
    REQUIRE(sprite->layer == 4);

    REQUIRE(actions.AddCamera(created.Value()));
    REQUIRE(scene.HasComponent<Janus::CameraComponent>(entity));
    REQUIRE(actions.SetCamera(created.Value(), 2.0f, true));
    REQUIRE(
        scene.GetComponent<Janus::CameraComponent>(entity)->primary);

    REQUIRE(actions.AddLuaScript(created.Value()));
    const auto* script =
        scene.GetComponent<Janus::LuaScriptComponent>(entity);
    REQUIRE(script != nullptr);
    REQUIRE_FALSE(script->script.IsValid());
    REQUIRE_FALSE(script->enabled);

    const auto invalidEnable =
        actions.SetLuaScriptEnabled(created.Value(), true);
    REQUIRE_FALSE(invalidEnable);
    REQUIRE(
        invalidEnable.GetError().code
        == Janus::ErrorCode::InvalidState);

    REQUIRE(actions.RemoveLuaScript(created.Value()));
    REQUIRE_FALSE(scene.HasComponent<Janus::LuaScriptComponent>(entity));

    REQUIRE(actions.RemoveCamera(created.Value()));
    REQUIRE_FALSE(scene.HasComponent<Janus::CameraComponent>(entity));

    REQUIRE(actions.RemoveSpriteRenderer(created.Value()));
    REQUIRE_FALSE(scene.HasComponent<Janus::SpriteRendererComponent>(entity));
}

TEST_CASE(
    "EditorActions reject authoring mutation during Play Mode",
    "[editor][actions][play][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    Janus::InputState input;
    REQUIRE(project->StartRuntime(input));

    const auto created = actions.CreateEntity("Forbidden");
    REQUIRE_FALSE(created);
    REQUIRE(
        created.GetError().code
        == Janus::ErrorCode::InvalidState);

    REQUIRE(project->StopRuntime());
}

TEST_CASE(
    "EditorActions primary Camera selection is unique",
    "[editor][actions][camera][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    auto& scene = project->GetEditorScene();

    const auto created = actions.CreateEntity("SecondCamera");
    REQUIRE(created);
    REQUIRE(actions.AddCamera(created.Value()));
    REQUIRE(actions.SetCamera(created.Value(), 1.5f, true));

    int primaryCount = 0;
    scene.View<Janus::CameraComponent>()
        .ForEach(
            [&](Janus::ECS::Entity, Janus::CameraComponent& camera)
            {
                if (camera.primary)
                {
                    ++primaryCount;
                }
            });

    REQUIRE(primaryCount == 1);
    const auto entity = scene.FindEntity(created.Value());
    REQUIRE(
        scene.GetComponent<Janus::CameraComponent>(entity)->primary);
}


TEST_CASE(
    "EditorActions mark ProjectSession dirty only after successful mutation",
    "[editor][actions][dirty][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    REQUIRE_FALSE(project->IsDirty());

    const auto invalidRename =
        actions.RenameEntity(
            Janus::UUID::Random(),
            "Missing");
    REQUIRE_FALSE(invalidRename);
    REQUIRE_FALSE(project->IsDirty());

    const auto created =
        actions.CreateEntity("Dirty");
    REQUIRE(created);
    REQUIRE(project->IsDirty());
}

TEST_CASE(
    "EditorActions assign only registered assets of the matching type",
    "[editor][actions][assets][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    const auto created =
        actions.CreateEntity("AssetTarget");
    REQUIRE(created);
    REQUIRE(actions.AddSpriteRenderer(created.Value()));
    REQUIRE(actions.AddLuaScript(created.Value()));

    const auto assets =
        project->GetAssetRegistry().GetAssets();

    Janus::AssetHandle texture;
    Janus::AssetHandle script;

    for (const auto& asset : assets)
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
            created.Value(),
            texture));
    REQUIRE(
        actions.SetLuaScriptAsset(
            created.Value(),
            script));

    auto& scene = project->GetEditorScene();
    const auto entity =
        scene.FindEntity(created.Value());

    REQUIRE(
        scene.GetComponent<Janus::SpriteRendererComponent>(
            entity)->texture
        == texture);
    REQUIRE(
        scene.GetComponent<Janus::LuaScriptComponent>(
            entity)->script
        == script);

    const auto wrongTexture =
        actions.SetSpriteTexture(
            created.Value(),
            script);
    REQUIRE_FALSE(wrongTexture);
    REQUIRE(
        wrongTexture.GetError().code
        == Janus::ErrorCode::AssetTypeMismatch);

    const auto wrongScript =
        actions.SetLuaScriptAsset(
            created.Value(),
            texture);
    REQUIRE_FALSE(wrongScript);
    REQUIRE(
        wrongScript.GetError().code
        == Janus::ErrorCode::AssetTypeMismatch);

    const auto missing =
        actions.SetSpriteTexture(
            created.Value(),
            Janus::AssetHandle::Random());
    REQUIRE_FALSE(missing);
    REQUIRE(
        missing.GetError().code
        == Janus::ErrorCode::AssetNotFound);
}


TEST_CASE(
    "EditorActions generic reflected property mutation is reversible",
    "[editor][actions][command][reflection][v0.7]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    const auto created =
        actions.CreateEntity("Reflected");
    REQUIRE(created);

    auto& scene =
        project->GetEditorScene();
    const auto entity =
        scene.FindEntity(created.Value());
    REQUIRE(entity.IsValid());

    REQUIRE(
        actions.SetProperty(
            created.Value(),
            Janus::SceneReflectionIds::Transform,
            Janus::SceneReflectionIds::TransformPosition,
            Janus::PropertyValue{
                Janus::Vector2{25.0f, 30.0f}}));

    REQUIRE(actions.CanUndo());
    REQUIRE_FALSE(actions.CanRedo());

    auto* transform =
        scene.GetComponent<Janus::TransformComponent>(
            entity);
    REQUIRE(transform != nullptr);
    REQUIRE(transform->position.x == Catch::Approx(25.0f));
    REQUIRE(transform->position.y == Catch::Approx(30.0f));

    REQUIRE(actions.Undo());
    REQUIRE(transform->position.x == Catch::Approx(0.0f));
    REQUIRE(transform->position.y == Catch::Approx(0.0f));
    REQUIRE(actions.CanRedo());

    REQUIRE(actions.Redo());
    REQUIRE(transform->position.x == Catch::Approx(25.0f));
    REQUIRE(transform->position.y == Catch::Approx(30.0f));
}

TEST_CASE(
    "EditorActions Undo and Redo mark a saved project dirty",
    "[editor][actions][command][dirty][v0.7]")
{
    ProjectTempDirectory temp;

    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project =
        OpenTempProject(
            *renderer,
            temp.Path());

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    const auto created =
        actions.CreateEntity("DirtyHistory");
    REQUIRE(created);
    REQUIRE(project->IsDirty());

    REQUIRE(project->SaveCurrentScene());
    REQUIRE_FALSE(project->IsDirty());

    REQUIRE(actions.Undo());
    REQUIRE(project->IsDirty());
    REQUIRE_FALSE(
        project->GetEditorScene()
            .FindEntity(created.Value())
            .IsValid());

    REQUIRE(project->SaveCurrentScene());
    REQUIRE_FALSE(project->IsDirty());

    REQUIRE(actions.Redo());
    REQUIRE(project->IsDirty());
    REQUIRE(
        project->GetEditorScene()
            .FindEntity(created.Value())
            .IsValid());
}

TEST_CASE(
    "EditorActions reject Undo and Redo during Play Mode",
    "[editor][actions][command][play][v0.7]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    const auto created =
        actions.CreateEntity("History");
    REQUIRE(created);
    REQUIRE(actions.CanUndo());

    const Janus::usize cursor =
        project->GetCommandBus().GetCursor();

    Janus::InputState input;
    REQUIRE(project->StartRuntime(input));

    REQUIRE_FALSE(actions.CanUndo());
    REQUIRE_FALSE(actions.CanRedo());

    const auto undone =
        actions.Undo();
    REQUIRE_FALSE(undone);
    REQUIRE(
        undone.GetError().code
        == Janus::ErrorCode::InvalidState);
    REQUIRE(
        project->GetCommandBus().GetCursor()
        == cursor);

    const auto redone =
        actions.Redo();
    REQUIRE_FALSE(redone);
    REQUIRE(
        redone.GetError().code
        == Janus::ErrorCode::InvalidState);
    REQUIRE(
        project->GetCommandBus().GetCursor()
        == cursor);

    REQUIRE(project->StopRuntime());
    REQUIRE(actions.CanUndo());
}

TEST_CASE(
    "EditorActions Undo restores contextual Camera primary mutation",
    "[editor][actions][command][camera][v0.7]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);

    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);

    auto& scene =
        project->GetEditorScene();

    Janus::ECS::Entity originalPrimary;
    scene.View<Janus::CameraComponent>()
        .ForEach(
            [&](Janus::ECS::Entity entity,
                Janus::CameraComponent& camera)
            {
                if (camera.primary)
                {
                    originalPrimary = entity;
                }
            });

    REQUIRE(originalPrimary.IsValid());

    const auto created =
        actions.CreateEntity("SecondPrimary");
    REQUIRE(created);
    REQUIRE(actions.AddCamera(created.Value()));

    REQUIRE(
        actions.SetProperty(
            created.Value(),
            Janus::SceneReflectionIds::Camera,
            Janus::SceneReflectionIds::CameraPrimary,
            Janus::PropertyValue{true}));

    const auto second =
        scene.FindEntity(created.Value());
    REQUIRE(second.IsValid());
    REQUIRE(
        scene.GetComponent<Janus::CameraComponent>(
            second)->primary);
    REQUIRE_FALSE(
        scene.GetComponent<Janus::CameraComponent>(
            originalPrimary)->primary);

    REQUIRE(actions.Undo());

    REQUIRE_FALSE(
        scene.GetComponent<Janus::CameraComponent>(
            second)->primary);
    REQUIRE(
        scene.GetComponent<Janus::CameraComponent>(
            originalPrimary)->primary);
}
