#include "../Renderer/FakeRenderDevice.h"
#include "EditorActions.h"
#include "EditorCloseController.h"
#include "EditorContext.h"
#include "EditorTransformDrag.h"
#include "ProjectSession.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Command/EntityCommands.h"
#include "Scene/Scene.h"
#include "Scene/SceneCloner.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneSerializer.h"
#include "UI/UIComponents.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <limits>

namespace
{
using namespace Janus;
using namespace Janus::Editor;
struct DragFixture
{
    Test::FakeRenderDevice device;
    std::unique_ptr<Renderer2D> renderer = Detail::Renderer2DTestAccess::Create(device);
    std::unique_ptr<ProjectSession> project;
    EditorContext context;
    EditorActions actions{context};
    TransformDragView view{EditorCamera{}, {800, 600}, {100, 50}, {400, 300}, 2};
    EditorTransformDrag drag;
    UUID target;

    DragFixture()
    {
        ProjectRuntimeConfig config;
        config.root = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "SandboxProject";
        auto opened = ProjectSession::Open(config, *renderer);
        REQUIRE(opened);
        project = std::move(opened).Value();
        context.project = project.get();
        auto entity = actions.CreateEntity("Move target");
        REQUIRE(entity);
        target = entity.Value();
    }
    TransformComponent& Transform(UUID id)
    {
        auto& scene = project->GetEditorScene();
        return *scene.GetComponent<TransformComponent>(scene.FindEntity(id));
    }
    void Begin(TransformDragAxis axis = TransformDragAxis::XY)
    {
        REQUIRE(drag.Begin(*project, target, axis, {300, 200}, view));
    }
};
} // namespace

TEST_CASE("Move drag maps display pixels once and commits one undoable command", "[editor][move]")
{
    DragFixture f;
    const auto history = f.project->GetCommandBus().GetHistorySize();
    const auto generation = f.project->GetAuthoringGeneration();
    const auto before =
        SceneSerializer::Serialize(f.project->GetEditorScene(), f.project->GetReflectionRegistry());
    REQUIRE(before);
    f.Begin();
    REQUIRE(f.drag.Update(*f.project, {310, 195}, f.view));
    REQUIRE(f.drag.GetPreview());
    CHECK(f.drag.GetPreview()->position.x == Catch::Approx(20));
    CHECK(f.drag.GetPreview()->position.y == Catch::Approx(10));
    CHECK(f.Transform(f.target).position.x == 0);
    const auto during =
        SceneSerializer::Serialize(f.project->GetEditorScene(), f.project->GetReflectionRegistry());
    REQUIRE(during);
    CHECK(during.Value() == before.Value());
    const auto clone =
        SceneCloner::Clone(f.project->GetEditorScene(), f.project->GetReflectionRegistry());
    REQUIRE(clone);
    const auto* clonedTransform =
        clone.Value()->GetComponent<TransformComponent>(clone.Value()->FindEntity(f.target));
    REQUIRE(clonedTransform);
    CHECK(clonedTransform->position.x == 0);
    CHECK(clonedTransform->position.y == 0);
    CHECK(f.project->GetAuthoringGeneration() == generation);
    CHECK(f.project->GetCommandBus().GetHistorySize() == history);
    REQUIRE(f.drag.Update(*f.project, {315, 190}, f.view));
    REQUIRE(f.drag.Commit(*f.project));
    CHECK_FALSE(f.drag.IsActive());
    CHECK(f.Transform(f.target).position.x == Catch::Approx(30));
    CHECK(f.Transform(f.target).position.y == Catch::Approx(20));
    CHECK(f.project->GetCommandBus().GetHistorySize() == history + 1);
    REQUIRE(f.actions.Undo());
    CHECK(f.Transform(f.target).position.x == 0);
    REQUIRE(f.actions.Redo());
    CHECK(f.Transform(f.target).position.x == Catch::Approx(30));
}

TEST_CASE("Move drag snaps world delta on constrained axes and releases snapping", "[editor][move]")
{
    DragFixture f;
    REQUIRE(f.actions.SetProperty(f.target, SceneReflectionIds::Transform,
                                  SceneReflectionIds::TransformPosition, Vector2{3, 7}));
    SECTION("X")
    {
        f.Begin(TransformDragAxis::X);
        REQUIRE(f.drag.Update(*f.project, {307, 180}, f.view, true, 10));
        CHECK(f.drag.GetWorldTarget().x == Catch::Approx(13));
        CHECK(f.drag.GetWorldTarget().y == Catch::Approx(7));
    }
    SECTION("Y")
    {
        f.Begin(TransformDragAxis::Y);
        REQUIRE(f.drag.Update(*f.project, {307, 194}, f.view, true, 10));
        CHECK(f.drag.GetWorldTarget().x == Catch::Approx(3));
        CHECK(f.drag.GetWorldTarget().y == Catch::Approx(17));
    }
    SECTION("XY and Ctrl release")
    {
        f.Begin();
        REQUIRE(f.drag.Update(*f.project, {307, 194}, f.view, true, 10));
        CHECK(f.drag.GetWorldTarget().x == Catch::Approx(13));
        CHECK(f.drag.GetWorldTarget().y == Catch::Approx(17));
        REQUIRE(f.drag.Update(*f.project, {307, 194}, f.view));
        CHECK(f.drag.GetWorldTarget().x == Catch::Approx(17));
        CHECK(f.drag.GetWorldTarget().y == Catch::Approx(19));
    }
}

TEST_CASE("Move drag inverts full rotated nonuniform parent chain", "[editor][move]")
{
    DragFixture f;
    const auto grand = f.actions.CreateEntity("Grandparent");
    const auto parent = f.actions.CreateEntity("Parent");
    REQUIRE(grand);
    REQUIRE(parent);
    REQUIRE(f.actions.SetTransform(grand.Value(), {12, -4}, 0.6f, {3, 0.5f}));
    REQUIRE(f.actions.SetTransform(parent.Value(), {-2, 9}, -0.9f, {0.7f, 2}));
    REQUIRE(f.actions.SetTransform(f.target, {4, -3}, 0, {1, 1}));
    REQUIRE(f.actions.ReparentEntity(parent.Value(), grand.Value()));
    REQUIRE(f.actions.ReparentEntity(f.target, parent.Value()));
    auto& scene = f.project->GetEditorScene();
    const auto start = ScenePose::Resolve(scene, f.target);
    REQUIRE(start);
    f.Begin(TransformDragAxis::X);
    REQUIRE(f.drag.Update(*f.project, {315, 180}, f.view));
    const auto preview = ScenePose::Resolve(scene, f.target, f.drag.GetPreview());
    REQUIRE(preview);
    CHECK(preview.Value().position.x == Catch::Approx(start.Value().position.x + 30).margin(0.001));
    CHECK(preview.Value().position.y == Catch::Approx(start.Value().position.y).margin(0.001));
    CHECK(f.Transform(f.target).position.x == 4);
    REQUIRE(f.drag.Commit(*f.project));
    REQUIRE(f.actions.Undo());
    CHECK(f.Transform(f.target).position.x == 4);
    CHECK(f.Transform(f.target).position.y == -3);
}

TEST_CASE("Move cancellation and no displacement leave authoring untouched", "[editor][move]")
{
    DragFixture f;
    const auto history = f.project->GetCommandBus().GetHistorySize();
    const auto generation = f.project->GetAuthoringGeneration();
    f.Begin();
    SECTION("Cancel")
    {
        REQUIRE(f.drag.Update(*f.project, {350, 170}, f.view));
        f.drag.Cancel();
    }
    SECTION("No movement")
    {
        REQUIRE(f.drag.Commit(*f.project));
    }
    SECTION("Return to start")
    {
        REQUIRE(f.drag.Update(*f.project, {350, 170}, f.view));
        REQUIRE(f.drag.Update(*f.project, {300, 200}, f.view));
        REQUIRE(f.drag.Commit(*f.project));
    }
    CHECK_FALSE(f.drag.IsActive());
    CHECK_FALSE(f.drag.GetPreview());
    CHECK(f.project->GetCommandBus().GetHistorySize() == history);
    CHECK(f.project->GetAuthoringGeneration() == generation);
    CHECK(f.Transform(f.target).position.x == 0);
}

TEST_CASE("Move cancels when camera viewport rectangle or DPI changes", "[editor][move]")
{
    DragFixture f;
    f.Begin();
    SECTION("Camera pan")
    {
        f.view.camera.PanPixels({1, 0});
    }
    SECTION("Camera zoom")
    {
        f.view.camera.Zoom(1);
    }
    SECTION("Render target")
    {
        f.view.viewport.width += 1;
    }
    SECTION("Display origin")
    {
        f.view.displayMin.x += 1;
    }
    SECTION("Display size")
    {
        f.view.displaySize.y += 1;
    }
    SECTION("DPI")
    {
        f.view.dpiScale = 1.5f;
    }
    CHECK_FALSE(f.drag.Validate(*f.project, f.view));
    CHECK_FALSE(f.drag.GetPreview());
    CHECK(f.Transform(f.target).position.x == 0);
}

TEST_CASE("Move rejects stale authoring including edit followed by undo", "[editor][move]")
{
    DragFixture f;
    f.Begin();
    REQUIRE(f.drag.Update(*f.project, {310, 195}, f.view));
    SECTION("Entity changed")
    {
        REQUIRE(f.actions.RenameEntity(f.target, "Changed"));
    }
    SECTION("Edit followed by undo")
    {
        REQUIRE(f.actions.RenameEntity(f.target, "Changed"));
        REQUIRE(f.actions.Undo());
    }
    SECTION("Parent changed without session notification")
    {
        auto& scene = f.project->GetEditorScene();
        const auto parent = scene.CreateEntity("Parent");
        REQUIRE(scene.SetParent(scene.FindEntity(f.target), parent));
    }
    SECTION("Local changed without session notification")
    {
        f.Transform(f.target).scale.x = 2;
    }
    CHECK_FALSE(f.drag.Commit(*f.project));
    CHECK_FALSE(f.drag.IsActive());
    CHECK(f.Transform(f.target).position.x == 0);
}

TEST_CASE("Move rejects invalid targets views and noninvertible parent matrices", "[editor][move]")
{
    DragFixture f;
    auto& scene = f.project->GetEditorScene();
    SECTION("Canvas")
    {
        scene.AddComponent(scene.FindEntity(f.target), CanvasComponent{});
    }
    SECTION("UIRect")
    {
        scene.AddComponent(scene.FindEntity(f.target), UIRectComponent{});
    }
    SECTION("Missing entity")
    {
        REQUIRE(scene.DestroyEntity(scene.FindEntity(f.target)));
    }
    SECTION("Empty viewport")
    {
        f.view.viewport.width = 0;
    }
    SECTION("Invalid display")
    {
        f.view.displaySize.x = -1;
    }
    SECTION("Nonfinite camera")
    {
        f.view.camera.PanPixels({std::numeric_limits<f32>::infinity(), 0});
    }
    SECTION("Singular parent")
    {
        const auto parent = scene.CreateEntity("Parent");
        scene.GetComponent<TransformComponent>(parent)->scale = {0, 1};
        REQUIRE(scene.SetParent(scene.FindEntity(f.target), parent));
    }
    SECTION("Nearly singular parent")
    {
        const auto parent = scene.CreateEntity("Parent");
        scene.GetComponent<TransformComponent>(parent)->scale = {0.00000001f, 1};
        REQUIRE(scene.SetParent(scene.FindEntity(f.target), parent));
    }
    CHECK_FALSE(f.drag.Begin(*f.project, f.target, TransformDragAxis::XY, {300, 200}, f.view));
    CHECK_FALSE(f.drag.IsActive());
}

TEST_CASE("Move cancels when close transaction or runtime blocks authoring", "[editor][move]")
{
    DragFixture f;
    f.Begin();
    EditorCloseController close(*f.project);
    SECTION("Close")
    {
        close.Request();
    }
    SECTION("Transaction")
    {
        REQUIRE(f.project->BeginAuthoringTransaction(UUID::Random()));
    }
    SECTION("Runtime")
    {
        REQUIRE(f.project->PlayRuntime(true));
    }
    SECTION("Runtime started and stopped between pointer updates")
    {
        REQUIRE(f.project->PlayRuntime(true));
        REQUIRE(f.project->StopRuntime());
    }
    CHECK_FALSE(f.drag.Commit(*f.project));
    CHECK_FALSE(f.drag.IsActive());
    CHECK(f.Transform(f.target).position.x == 0);
}

TEST_CASE("Move cancellation catches scene replacement deletion and changed ancestors",
          "[editor][move]")
{
    DragFixture f;
    auto& scene = f.project->GetEditorScene();
    const auto parent = f.actions.CreateEntity("Parent");
    REQUIRE(parent);
    REQUIRE(f.actions.ReparentEntity(f.target, parent.Value()));
    f.Begin();
    SECTION("Ancestor changed directly")
    {
        f.Transform(parent.Value()).rotationRadians = 0.4f;
    }
    SECTION("Ancestor deleted")
    {
        REQUIRE(scene.DestroyEntity(scene.FindEntity(parent.Value())));
    }
    SECTION("Agent deleted target")
    {
        REQUIRE(f.project->ExecuteAuthoring(
            std::make_unique<DeleteEntityCommand>(
                scene, SceneReflection(f.project->GetReflectionRegistry()), f.target),
            CommandActor::Agent));
    }
    SECTION("Scene replaced")
    {
        REQUIRE(f.project->DiscardUnsavedAndReload());
    }
    CHECK_FALSE(f.drag.Validate(*f.project, f.view));
    CHECK_FALSE(f.drag.IsActive());
}

TEST_CASE("Move rejects another project and invalid updates without authoring writes",
          "[editor][move]")
{
    DragFixture f;
    const auto generation = f.project->GetAuthoringGeneration();
    f.Begin();
    SECTION("Another project")
    {
        ProjectRuntimeConfig config;
        config.root = f.project->GetProjectRoot();
        auto other = ProjectSession::Open(config, *f.renderer);
        REQUIRE(other);
        CHECK_FALSE(f.drag.Commit(*other.Value()));
    }
    SECTION("Invalid pointer")
    {
        CHECK_FALSE(
            f.drag.Update(*f.project, {std::numeric_limits<f32>::quiet_NaN(), 200}, f.view));
    }
    SECTION("Invalid grid")
    {
        CHECK_FALSE(f.drag.Update(*f.project, {310, 200}, f.view, true, 0));
    }
    CHECK_FALSE(f.drag.IsActive());
    CHECK(f.project->GetAuthoringGeneration() == generation);
    CHECK(f.Transform(f.target).position.x == 0);
}
