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
