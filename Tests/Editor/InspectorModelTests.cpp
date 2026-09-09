#include "../Renderer/FakeRenderDevice.h"
#include "EditorActions.h"
#include "EditorContext.h"
#include "InspectorModel.h"
#include "ProjectSession.h"
#include "Renderer/Renderer2D.h"
#include "Runtime/RuntimeSession.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string_view>
#include <utility>

namespace
{

const Janus::Editor::InspectorComponentModel* FindComponent(
    const std::vector<Janus::Editor::InspectorComponentModel>& model,
    std::string_view name)
{
    const auto found =
        std::find_if(
            model.begin(),
            model.end(),
            [name](const Janus::Editor::InspectorComponentModel& component)
            {
                return component.descriptor != nullptr
                    && component.descriptor->name == name;
            });

    return found == model.end()
        ? nullptr
        : &*found;
}

bool HasProperty(
    const Janus::Editor::InspectorComponentModel& component,
    std::string_view name)
{
    return std::any_of(
        component.properties.begin(),
        component.properties.end(),
        [name](const Janus::Editor::InspectorPropertyModel& property)
        {
            return property.descriptor != nullptr
                && property.descriptor->name == name;
        });
}

} // namespace

TEST_CASE(
    "Inspector model enumerates reflected component presence and visible properties",
    "[editor][inspector][reflection][v0.7]")
{
    auto registryResult =
        Janus::CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registryResult);

    auto registry =
        std::move(registryResult).Value();

    Janus::Scene scene;
    const auto entity =
        scene.CreateEntity("Inspectable");

    const auto* identity =
        scene.GetComponent<Janus::EntityIdentityComponent>(
            entity);
    REQUIRE(identity != nullptr);

    scene.AddComponent<Janus::SpriteRendererComponent>(
        entity,
        Janus::SpriteRendererComponent{});

    const auto modelResult =
        Janus::Editor::BuildInspectorModel(
            scene,
            identity->id,
            registry);
    REQUIRE(modelResult);

    const auto& model =
        modelResult.Value();

    REQUIRE(model.size() == 14);

    const auto* transform =
        FindComponent(
            model,
            "Transform");
    REQUIRE(transform != nullptr);
    REQUIRE(transform->present);
    REQUIRE(transform->properties.size() == 3);
    REQUIRE(HasProperty(*transform, "position"));
    REQUIRE(HasProperty(*transform, "rotation"));
    REQUIRE(HasProperty(*transform, "scale"));

    const auto* sprite =
        FindComponent(
            model,
            "SpriteRenderer");
    REQUIRE(sprite != nullptr);
    REQUIRE(sprite->present);
    REQUIRE(HasProperty(*sprite, "texture"));
    REQUIRE(HasProperty(*sprite, "size"));
    REQUIRE(HasProperty(*sprite, "color"));
    REQUIRE(HasProperty(*sprite, "layer"));
    REQUIRE(HasProperty(*sprite, "enabled"));
    REQUIRE_FALSE(HasProperty(*sprite, "uvMin"));
    REQUIRE_FALSE(HasProperty(*sprite, "uvMax"));

    const auto* camera =
        FindComponent(
            model,
            "Camera");
    REQUIRE(camera != nullptr);
    REQUIRE_FALSE(camera->present);
    REQUIRE(camera->properties.empty());
    REQUIRE(camera->descriptor->removable);

    const auto* script =
        FindComponent(
            model,
            "LuaScript");
    REQUIRE(script != nullptr);
    REQUIRE_FALSE(script->present);
    REQUIRE(script->properties.empty());
    REQUIRE(script->descriptor->removable);
}

TEST_CASE(
    "Inspector model rejects invalid entity identity",
    "[editor][inspector][reflection][errors][v0.7]")
{
    auto registryResult =
        Janus::CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registryResult);

    Janus::Scene scene;

    const auto invalid =
        Janus::Editor::BuildInspectorModel(
            scene,
            Janus::UUID{},
            registryResult.Value());
    REQUIRE_FALSE(invalid);
    REQUIRE(
        invalid.GetError().code
        == Janus::ErrorCode::InvalidArgument);

    const auto missing =
        Janus::Editor::BuildInspectorModel(
            scene,
            Janus::UUID::Random(),
            registryResult.Value());
    REQUIRE_FALSE(missing);
    REQUIRE(
        missing.GetError().code
        == Janus::ErrorCode::EntityNotFound);
}

namespace
{
struct DraftFixture
{
    Janus::Test::FakeRenderDevice device;
    std::unique_ptr<Janus::Renderer2D> renderer =
        Janus::Detail::Renderer2DTestAccess::Create(device);
    std::unique_ptr<Janus::Editor::ProjectSession> project;
    Janus::Editor::EditorContext context;
    Janus::Editor::EditorActions actions{context};
    Janus::Editor::InspectorEditDraft draft;
    Janus::UUID entity;
    DraftFixture()
    {
        Janus::ProjectRuntimeConfig config;
        config.root = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "SandboxProject";
        auto opened = Janus::Editor::ProjectSession::Open(config, *renderer);
        REQUIRE(opened);
        project = std::move(opened).Value();
        context.project = project.get();
        const auto created = actions.CreateEntity("Draft entity");
        REQUIRE(created);
        entity = created.Value();
    }
    void BeginPosition()
    {
        REQUIRE(draft.BeginProperty(*project, entity, Janus::SceneReflectionIds::Transform,
                                    Janus::SceneReflectionIds::TransformPosition,
                                    Janus::Vector2{}));
    }
};
} // namespace

TEST_CASE("Inspector draft accumulates edits and commits once through shared history",
          "[inspector-draft]")
{
    using namespace Janus;
    DraftFixture f;
    const auto size = f.project->GetCommandBus().GetHistorySize();
    const auto generation = f.project->GetAuthoringGeneration();
    f.BeginPosition();
    f.draft.SetValue(Vector2{1, 2});
    f.draft.SetValue(Vector2{5, 8});
    CHECK(f.project->GetCommandBus().GetHistorySize() == size);
    CHECK(f.project->GetAuthoringGeneration() == generation);
    REQUIRE(f.draft.Commit(*f.project, f.actions));
    CHECK_FALSE(f.draft.IsActive());
    CHECK(f.project->GetCommandBus().GetHistorySize() == size + 1);
    REQUIRE(f.draft.Commit(*f.project, f.actions));
    CHECK(f.project->GetCommandBus().GetHistorySize() == size + 1);
    auto& scene = f.project->GetEditorScene();
    CHECK(scene.GetComponent<TransformComponent>(scene.FindEntity(f.entity))->position.y == 8);
    REQUIRE(f.actions.Undo());
    CHECK(scene.GetComponent<TransformComponent>(scene.FindEntity(f.entity))->position.y == 0);
}

TEST_CASE("Inspector draft cancel and unchanged values never enter history", "[inspector-draft]")
{
    using namespace Janus;
    DraftFixture f;
    const auto generation = f.project->GetAuthoringGeneration();
    f.BeginPosition();
    SECTION("Cancel")
    {
        f.draft.SetValue(Vector2{5, 8});
        f.draft.Cancel();
    }
    SECTION("Return to original")
    {
        f.draft.SetValue(Vector2{5, 8});
        f.draft.SetValue(Vector2{});
        CHECK_FALSE(f.draft.IsEdited());
        REQUIRE(f.draft.Commit(*f.project, f.actions));
    }
    CHECK_FALSE(f.draft.IsActive());
    CHECK(f.project->GetAuthoringGeneration() == generation);
}

TEST_CASE("Settled Inspector values enter the Runtime clone and remain undoable after Stop",
          "[inspector-draft]")
{
    using namespace Janus;
    DraftFixture f;
    f.BeginPosition();
    f.draft.SetValue(Vector2{11, 7});
    REQUIRE(f.draft.Commit(*f.project, f.actions));
    REQUIRE(f.project->StartRuntime({}));
    auto& runtimeScene = f.project->GetRuntimeSession()->GetScene();
    const auto runtimeEntity = runtimeScene.FindEntity(f.entity);
    REQUIRE(runtimeEntity.IsValid());
    CHECK(runtimeScene.GetComponent<TransformComponent>(runtimeEntity)->position.x == 11);
    CHECK(runtimeScene.GetComponent<TransformComponent>(runtimeEntity)->position.y == 7);
    REQUIRE(f.project->StopRuntime());
    REQUIRE(f.actions.Undo());
    auto& editorScene = f.project->GetEditorScene();
    CHECK(editorScene.GetComponent<TransformComponent>(editorScene.FindEntity(f.entity))->position.x == 0);
}

TEST_CASE("Inspector invalid draft remains available for correction after failed commit",
          "[inspector-draft]")
{
    using namespace Janus;
    DraftFixture f;
    REQUIRE(f.actions.AddCamera(f.entity));
    REQUIRE(f.draft.BeginProperty(*f.project, f.entity, SceneReflectionIds::Camera,
                                  SceneReflectionIds::CameraZoom, f32{1}));
    f.draft.SetValue(f32{0});
    CHECK_FALSE(f.draft.Commit(*f.project, f.actions));
    CHECK(f.draft.IsActive());
    CHECK(f.draft.IsEdited());
    CHECK(f.draft.GetError());
    f.draft.SetValue(f32{2});
    REQUIRE(f.draft.Commit(*f.project, f.actions));
    CHECK_FALSE(f.draft.IsActive());
}

TEST_CASE("Inspector draft rejects stale authoring without losing typed value", "[inspector-draft]")
{
    using namespace Janus;
    DraftFixture f;
    f.BeginPosition();
    f.draft.SetValue(Vector2{5, 8});
    SECTION("Another authoring edit")
    {
        REQUIRE(f.actions.RenameEntity(f.entity, "Changed"));
    }
    SECTION("Edit then Undo")
    {
        REQUIRE(f.actions.RenameEntity(f.entity, "Changed"));
        REQUIRE(f.actions.Undo());
    }
    SECTION("Runtime started then stopped")
    {
        REQUIRE(f.project->PlayRuntime(true));
        REQUIRE(f.project->StopRuntime());
    }
    SECTION("Transaction in progress")
    {
        REQUIRE(f.project->BeginAuthoringTransaction(UUID::Random()));
    }
    CHECK_FALSE(f.draft.Commit(*f.project, f.actions));
    REQUIRE(f.draft.IsActive());
    CHECK(std::get<Vector2>(f.draft.GetValue()).x == 5);
    CHECK(f.draft.GetError());
    f.draft.Cancel();
    CHECK_FALSE(f.draft.GetError());
}

TEST_CASE("Inspector draft preserves names text and session identity", "[inspector-draft]")
{
    using namespace Janus;
    DraftFixture f;
    REQUIRE(f.draft.BeginName(*f.project, f.entity, "Draft entity"));
    f.draft.SetValue(std::string{"Renamed entity"});
    SECTION("Rename commits")
    {
        REQUIRE(f.draft.Commit(*f.project, f.actions));
        const auto& scene = f.project->GetEditorScene();
        CHECK(scene.GetComponent<EntityIdentityComponent>(scene.FindEntity(f.entity))->name ==
              "Renamed entity");
    }
    SECTION("Same path reopened")
    {
        ProjectRuntimeConfig config;
        config.root = f.project->GetProjectRoot();
        auto other = Editor::ProjectSession::Open(config, *f.renderer);
        REQUIRE(other);
        Editor::EditorContext otherContext;
        otherContext.project = other.Value().get();
        Editor::EditorActions otherActions(otherContext);
        CHECK_FALSE(f.draft.Commit(*other.Value(), otherActions));
        CHECK(std::get<std::string>(f.draft.GetValue()) == "Renamed entity");
    }
    SECTION("Rebinding edited draft rejected")
    {
        CHECK_FALSE(f.draft.BeginProperty(*f.project, f.entity, SceneReflectionIds::Transform,
                                          SceneReflectionIds::TransformPosition, Vector2{}));
        CHECK(std::get<std::string>(f.draft.GetValue()) == "Renamed entity");
    }
}

TEST_CASE("Inspector text draft preserves multiline content and rejects oversized values",
          "[inspector-draft]")
{
    using namespace Janus;
    DraftFixture f;
    REQUIRE(f.actions.AddComponent(f.entity, SceneReflectionIds::Text));
    const auto content = MakePropertyId("Text.content");
    REQUIRE(f.draft.BeginProperty(*f.project, f.entity, SceneReflectionIds::Text, content,
                                  std::string{}));
    f.draft.SetValue(std::string(4097, 'x'));
    CHECK_FALSE(f.draft.Commit(*f.project, f.actions));
    REQUIRE(f.draft.IsEdited());
    CHECK(std::get<std::string>(f.draft.GetValue()).size() == 4097);
    f.draft.SetValue(std::string{"First line\nSecond line"});
    REQUIRE(f.draft.Commit(*f.project, f.actions));
    const auto model = Editor::BuildInspectorModel(f.project->GetEditorScene(), f.entity,
                                                   f.project->GetReflectionRegistry());
    REQUIRE(model);
    const auto* text = FindComponent(model.Value(), "Text");
    REQUIRE(text);
    const auto value = std::find_if(text->properties.begin(), text->properties.end(),
                                    [content](const auto& property)
                                    { return property.descriptor->id == content; });
    REQUIRE(value != text->properties.end());
    CHECK(std::get<std::string>(value->value) == "First line\nSecond line");
}
