#include "EditorActions.h"
#include "EditorContext.h"
#include "EditorTransformDrag.h"
#include "ProjectSession.h"

#include "Application/ApplicationConfig.h"
#include "Core/Input/InputState.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"
#include "UI/UIComponents.h"

#include "../Renderer/FakeRenderDevice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
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

TEST_CASE("Move drag uses Human actions and one reversible command", "[editor][move]")
{
    using namespace Janus;
    using namespace Janus::Editor;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);
    EditorContext context;
    context.project = project.get();
    EditorActions actions(context);
    auto id = actions.CreateEntity("Move target");
    REQUIRE(id);
    TransformDragView view;
    view.viewport = {640, 360};
    view.displaySize = {640, 360};
    EditorTransformDrag drag;
    REQUIRE(drag.Begin(*project, id.Value(), TransformDragAxis::X, {320, 180}, view));
    REQUIRE(drag.Update(*project, {360, 200}, view));
    const auto entity = project->GetEditorScene().FindEntity(id.Value());
    CHECK(project->GetEditorScene().GetComponent<TransformComponent>(entity)->position.x == 0);
    REQUIRE(actions.CommitTransformDrag(drag));
    CHECK_FALSE(drag.IsActive());
    CHECK(project->GetEditorScene().GetComponent<TransformComponent>(entity)->position.x == 40);
    REQUIRE(actions.Undo());
    CHECK(project->GetEditorScene().GetComponent<TransformComponent>(entity)->position.x == 0);
    REQUIRE(actions.Redo());
    CHECK(project->GetEditorScene().GetComponent<TransformComponent>(entity)->position.x == 40);
}

TEST_CASE("Editor reparent shares command history and Runtime guard", "[ui][editor]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);
    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);
    const auto child = actions.CreateEntity("Child");
    const auto parent = actions.CreateEntity("Parent");
    REQUIRE(child);
    REQUIRE(parent);
    REQUIRE(actions.ReparentEntity(child.Value(), parent.Value()));
    auto& scene = project->GetEditorScene();
    CHECK(scene.GetComponent<Janus::HierarchyComponent>(scene.FindEntity(child.Value()))->parent ==
          scene.FindEntity(parent.Value()));
    REQUIRE(actions.Undo());
    CHECK_FALSE(scene.GetComponent<Janus::HierarchyComponent>(scene.FindEntity(child.Value()))
                    ->parent.IsValid());
    REQUIRE(actions.Redo());
    Janus::InputState input;
    REQUIRE(project->StartRuntime(input));
    CHECK_FALSE(actions.ReparentEntity(child.Value(), {}));
    REQUIRE(project->StopRuntime());
}

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
        if (asset.relativePath == "Assets/player.png")
        {
            texture = asset.handle;
        }
        else if (asset.relativePath == "Scripts/PlayerController.lua")
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


TEST_CASE(
    "ProjectSession command history is scoped to one authoring session",
    "[editor][actions][command][session][v0.7]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);

    auto first = OpenProject(*renderer);

    Janus::Editor::EditorContext firstContext;
    firstContext.project = first.get();
    Janus::Editor::EditorActions firstActions(firstContext);

    REQUIRE(firstActions.CreateEntity("SessionOnly"));
    REQUIRE(
        first->GetCommandBus().GetHistorySize()
        == 1);

    auto second = OpenProject(*renderer);
    REQUIRE(
        second->GetCommandBus().GetHistorySize()
        == 0);
    REQUIRE(
        second->GetCommandBus().GetCursor()
        == 0);
}

TEST_CASE("Editor Text font assignment validates asset type supports undo and Runtime guard",
          "[ui][text][editor]")
{
    using namespace Janus;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);
    Editor::EditorContext context;
    context.project = project.get();
    Editor::EditorActions actions(context);
    auto id = actions.CreateEntity("TextTarget");
    REQUIRE(id);
    REQUIRE(actions.AddComponent(id.Value(), MakeComponentTypeId("Text")));
    const auto* font = project->GetAssetRegistry().FindByPath("Fonts/JanusPixel.font.json");
    REQUIRE(font);
    REQUIRE(actions.SetProperty(id.Value(), MakeComponentTypeId("Text"),
                                MakePropertyId("Text.font"), AssetReferenceValue{font->handle.id}));
    auto* text = project->GetEditorScene().GetComponent<TextComponent>(
        project->GetEditorScene().FindEntity(id.Value()));
    CHECK(text->font.id == font->handle.id);
    REQUIRE(actions.Undo());
    CHECK_FALSE(text->font.id.IsValid());
    REQUIRE(actions.Redo());
    const auto* wrong = project->GetAssetRegistry().FindByPath("Assets/player.png");
    REQUIRE(wrong);
    CHECK_FALSE(actions.SetProperty(id.Value(), MakeComponentTypeId("Text"),
                                    MakePropertyId("Text.font"),
                                    AssetReferenceValue{wrong->handle.id}));
    CHECK(text->font.id == font->handle.id);
    InputState input;
    REQUIRE(project->StartRuntime(input));
    CHECK_FALSE(actions.SetProperty(id.Value(), MakeComponentTypeId("Text"),
                                    MakePropertyId("Text.content"), std::string("Blocked")));
    REQUIRE(project->StopRuntime());
    CHECK(text->content.empty());
}

TEST_CASE("Editor Button authoring uses undo validation and Runtime guards", "[ui][button][editor]")
{
    using namespace Janus;
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenProject(*renderer);
    Editor::EditorContext context;
    context.project = project.get();
    Editor::EditorActions actions(context);
    auto id = actions.CreateEntity("ButtonTarget");
    REQUIRE(id);
    REQUIRE(actions.AddComponent(id.Value(), MakeComponentTypeId("Button")));
    REQUIRE(actions.SetProperty(id.Value(), MakeComponentTypeId("Button"),
                                MakePropertyId("Button.interactable"), false));
    auto button = [&]
    {
        return project->GetEditorScene().GetComponent<ButtonComponent>(
            project->GetEditorScene().FindEntity(id.Value()));
    };
    CHECK_FALSE(button()->interactable);
    REQUIRE(actions.Undo());
    CHECK(button()->interactable);
    REQUIRE(actions.Redo());
    CHECK_FALSE(button()->interactable);
    CHECK_FALSE(actions.SetProperty(id.Value(), MakeComponentTypeId("Button"),
                                    MakePropertyId("Button.color"), ColorValue{2, 0, 0, 1}));
    InputState input;
    REQUIRE(project->StartRuntime(input));
    CHECK_FALSE(actions.SetProperty(id.Value(), MakeComponentTypeId("Button"),
                                    MakePropertyId("Button.enabled"), false));
    REQUIRE(project->StopRuntime());
    CHECK(button()->enabled);
    REQUIRE(actions.RemoveComponent(id.Value(), MakeComponentTypeId("Button")));
    CHECK_FALSE(button());
    REQUIRE(actions.Undo());
    REQUIRE(button());
    CHECK_FALSE(button()->interactable);
}

TEST_CASE("Named Prefab exports preserve history and survive registry reopen",
          "[asset-workflow][prefab]")
{
    ProjectTempDirectory temp;
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenTempProject(*renderer, temp.Path());
    Janus::Editor::EditorContext context;
    context.project = project.get();
    Janus::Editor::EditorActions actions(context);
    auto entity = actions.CreateEntity("Fighter");
    REQUIRE(entity);
    REQUIRE(project->SaveCurrentScene());
    const auto history = project->GetCommandBus().GetHistorySize();
    const auto count = project->GetAssetRegistry().Size();
    REQUIRE_FALSE(actions.ExportPrefab(entity.Value(), "../bad"));
    REQUIRE(project->GetAssetRegistry().Size() == count);
    auto exported = actions.ExportPrefab(entity.Value(), "Fighter Robot");
    REQUIRE(exported);
    const auto* metadata = project->GetAssetRegistry().Find(exported.Value());
    REQUIRE(metadata);
    REQUIRE(metadata->relativePath.generic_string() ==
            "Prefabs/Fighter Robot-" + exported.Value().ToString() + ".prefab");
    REQUIRE(context.locateAsset == exported.Value().id);
    REQUIRE_FALSE(project->IsDirty());
    REQUIRE(project->GetCommandBus().GetHistorySize() == history);
    auto reopened = OpenTempProject(*renderer, temp.Path());
    REQUIRE(reopened->GetAssetRegistry().Contains(exported.Value()));
    auto unicode = actions.ExportPrefab(entity.Value(), "\xe6\x9c\xba\xe5\x99\xa8\xe4\xba\xba");
    REQUIRE(unicode);
    REQUIRE(OpenTempProject(*renderer, temp.Path())->GetAssetRegistry().Contains(unicode.Value()));
    auto second = actions.ExportPrefab(entity.Value(), "Fighter Robot");
    REQUIRE(second);
    REQUIRE(second.Value() != exported.Value());
    auto legacy = actions.ExportPrefab(entity.Value());
    REQUIRE(legacy);
    REQUIRE(project->GetAssetRegistry().Find(legacy.Value())->relativePath.filename().string() ==
            legacy.Value().ToString() + ".prefab");
}

TEST_CASE("Asset payload assignment validates all six slots and project identity",
          "[asset-workflow]")
{
    using namespace Janus;
    ProjectTempDirectory temp;
    auto assets = AssetRegistry::Load(temp.Path() / "Config/AssetRegistry.json");
    REQUIRE(assets);
    const AssetType types[] = {AssetType::Texture,   AssetType::Texture,
                               AssetType::Font,      AssetType::AnimationClip,
                               AssetType::AudioClip, AssetType::LuaScript};
    const char* components[] = {"SpriteRenderer", "Image",       "Text",
                                "Animator",       "AudioSource", "LuaScript"};
    const char* properties[] = {"SpriteRenderer.texture", "Image.texture",    "Text.font",
                                "Animator.clip",          "AudioSource.clip", "LuaScript.script"};
    std::vector<AssetHandle> handles;
    for (int i = 0; i < 6; ++i)
    {
        auto asset = assets.Value().Register(types[i], "Assets/slot" + std::to_string(i));
        REQUIRE(asset);
        handles.push_back(asset.Value());
    }
    REQUIRE(assets.Value().Save(temp.Path() / "Config/AssetRegistry.json"));
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    auto project = OpenTempProject(*renderer, temp.Path());
    Editor::EditorContext context;
    context.project = project.get();
    Editor::EditorActions actions(context);
    auto entity = actions.CreateEntity("Asset slots");
    REQUIRE(entity);
    for (int i = 0; i < 6; ++i)
    {
        INFO(components[i]);
        const auto component = MakeComponentTypeId(components[i]);
        const auto property = MakePropertyId(properties[i]);
        REQUIRE(actions.AddComponent(entity.Value(), component));
        const auto history = project->GetCommandBus().GetHistorySize();
        REQUIRE_FALSE(actions.AssignAssetPayload(entity.Value(), component, property,
                                                 {UUID::Random(), handles[i].id}));
        REQUIRE_FALSE(actions.AssignAssetPayload(entity.Value(), component, property,
                                                 {project->GetProjectIdentity(), UUID::Random()}));
        const auto wrong = handles[i < 2 ? 2 : 0];
        REQUIRE_FALSE(actions.AssignAssetPayload(entity.Value(), component, property,
                                                 {project->GetProjectIdentity(), wrong.id}));
        REQUIRE(project->GetCommandBus().GetHistorySize() == history);
        REQUIRE(actions.AssignAssetPayload(entity.Value(), component, property,
                                           {project->GetProjectIdentity(), handles[i].id}));
        SceneReflection reflected(project->GetReflectionRegistry(), &project->GetAssetRegistry());
        auto value =
            reflected.GetProperty(project->GetEditorScene(), entity.Value(), component, property);
        REQUIRE(value);
        REQUIRE(std::get<AssetReferenceValue>(value.Value()).id == handles[i].id);
        REQUIRE(actions.Undo());
        REQUIRE(actions.Redo());
        REQUIRE(actions.SetProperty(entity.Value(), component, property, AssetReferenceValue{}));
        auto cleared =
            reflected.GetProperty(project->GetEditorScene(), entity.Value(), component, property);
        REQUIRE(cleared);
        REQUIRE_FALSE(std::get<AssetReferenceValue>(cleared.Value()).id.IsValid());
        REQUIRE(actions.Undo());
    }
    auto duplicate = actions.DuplicateEntity(entity.Value());
    REQUIRE(duplicate);
    REQUIRE(context.selection.GetSelectedUUID() == duplicate.Value());
    SceneReflection reflected(project->GetReflectionRegistry(), &project->GetAssetRegistry());
    for (int i = 0; i < 6; ++i)
    {
        auto value = reflected.GetProperty(project->GetEditorScene(), duplicate.Value(),
                                           MakeComponentTypeId(components[i]),
                                           MakePropertyId(properties[i]));
        REQUIRE(value);
        REQUIRE(std::get<AssetReferenceValue>(value.Value()).id == handles[i].id);
    }
    const auto owner = UUID::Random();
    auto token = project->BeginAuthoringTransaction(owner);
    REQUIRE(token);
    REQUIRE_FALSE(actions.DuplicateEntity(entity.Value()));
    REQUIRE_FALSE(actions.AssignAssetPayload(entity.Value(), MakeComponentTypeId("Image"),
                                             MakePropertyId("Image.texture"),
                                             {project->GetProjectIdentity(), handles[1].id}));
    REQUIRE(project->FinishAuthoringTransaction(token.Value(), owner, false));
    REQUIRE(project->SaveCurrentScene());
    auto reopened = OpenTempProject(*renderer, temp.Path());
    SceneReflection savedReflection(reopened->GetReflectionRegistry(),
                                    &reopened->GetAssetRegistry());
    for (int i = 0; i < 6; ++i)
    {
        auto value = savedReflection.GetProperty(reopened->GetEditorScene(), duplicate.Value(),
                                                 MakeComponentTypeId(components[i]),
                                                 MakePropertyId(properties[i]));
        REQUIRE(value);
        REQUIRE(std::get<AssetReferenceValue>(value.Value()).id == handles[i].id);
    }
}
