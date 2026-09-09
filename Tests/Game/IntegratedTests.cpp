#include "../Asset/AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"
#include "Core/FileSystem/FileSystem.h"
#include "EditorActions.h"
#include "EditorContext.h"
#include "Physics/PhysicsSystem.h"
#include "ProjectSession.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "Tools/SceneTools.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Integrated combat hot reload preserves arena bindings and physical feedback",
          "[acceptance][game][reload]")
{
    Janus::Test::AssetTempDirectory temp;
    const auto source = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "Game";
    std::filesystem::copy(source, temp.Path(), std::filesystem::copy_options::recursive);
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::ProjectRuntimeConfig config;
    config.root = temp.Path();
    auto opened = Janus::Editor::ProjectSession::Open(config, *renderer);
    REQUIRE(opened);
    auto& project = *opened.Value();
    Janus::InputState input;
    REQUIRE(project.StartRuntime(input));
    const auto run = project.GetRuntimeStatus().runtimeId;
    auto buttonEnabled = [&](const char* name)
    {
        auto& scene = project.GetRuntimeSession()->GetScene();
        for (const auto entity : scene.GetEntities())
            if (scene.GetComponent<Janus::EntityIdentityComponent>(entity)->name == name)
                return scene.GetComponent<Janus::ButtonComponent>(entity)->interactable;
        CHECK(false);
        return false;
    };
    CHECK(buttonEnabled("Start"));
    CHECK_FALSE(buttonEnabled("Play"));
    const auto path = temp.Path() / "Scripts/Combat.lua";
    auto text = Janus::FileSystem::ReadText(path);
    REQUIRE(text);
    const auto previous = std::filesystem::last_write_time(path);
    REQUIRE(Janus::FileSystem::WriteText(path, text.Value() + "\n-- reload regression\n"));
    std::filesystem::last_write_time(path, previous + std::chrono::seconds(2));
    REQUIRE(project.UpdateRuntime(Janus::TimeStep::FromSeconds(1.0 / 60.0)));
    auto snapshot = project.GetRuntimeSession()->GetSnapshot();
    REQUIRE(snapshot);
    REQUIRE(snapshot->fields.contains("hits"));
    CHECK(std::get<std::string>(snapshot->fields.at("phase")) == "menu");
    CHECK(buttonEnabled("Start"));
    CHECK_FALSE(buttonEnabled("Play"));
    CHECK_FALSE(buttonEnabled("Strike"));
    for (auto key : {Janus::KeyCode::Digit1, Janus::KeyCode::Digit2, Janus::KeyCode::Digit4})
    {
        input.BeginFrame();
        input.Apply(Janus::KeyPressedEvent{key, false});
        input.Apply(Janus::KeyReleasedEvent{key});
        REQUIRE(project.UpdateRuntime(Janus::TimeStep::FromSeconds(1.0 / 60.0)));
    }
    snapshot = project.GetRuntimeSession()->GetSnapshot();
    CHECK(std::get<double>(snapshot->fields.at("enemyHp")) == 8);
    CHECK_FALSE(buttonEnabled("Start"));
    CHECK(buttonEnabled("Strike"));
    CHECK_FALSE(buttonEnabled("Play"));
    CHECK(std::get<double>(snapshot->fields.at("hits")) == 2);
    CHECK(project.GetRuntimeStatus().runtimeId == run);
    CHECK(project.GetRuntimeSession()->GetPhysics().GetBodyCount() == 3);
    CHECK_FALSE(project.IsDirty());
}

TEST_CASE("Integrated Agent prefab authoring remains Human undoable after play and save",
          "[acceptance][prefab][editor][mcp]")
{
    Janus::Test::AssetTempDirectory temp;
    const auto source = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "Game";
    std::filesystem::copy(source, temp.Path(), std::filesystem::copy_options::recursive);
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::ProjectRuntimeConfig config;
    config.root = temp.Path();
    auto opened = Janus::Editor::ProjectSession::Open(config, *renderer);
    REQUIRE(opened);
    auto& project = *opened.Value();
    const auto count = project.GetEditorScene().GetEntities().size();
    const auto owner = Janus::UUID::Random();
    Janus::MCP::ToolRegistry tools;
    Janus::MCP::McpSceneToolContext context{
        &project.GetEditorScene(),
        &project.GetReflectionRegistry(),
        &project.GetCommandBus(),
        &project.GetAssetRegistry(),
        [&] { return project.SaveCurrentScene(); },
        [&] { project.MarkDirty(); },
        [&] { return project.HasRuntime(); },
        [&](std::unique_ptr<Janus::ICommand> command, Janus::UUID token)
        {
            return project.ExecuteAuthoring(std::move(command), Janus::CommandActor::Agent, token,
                                            owner);
        },
        temp.Path(),
        [&](Janus::UUID root, std::optional<std::string> name)
        { return project.ExportPrefab(root, std::move(name)); }};
    REQUIRE(Janus::MCP::RegisterSceneTools(tools, context));
    auto token = project.BeginAuthoringTransaction(owner);
    REQUIRE(token);
    auto response = tools.HandleCall({{"name", "scene.instantiate_prefab"},
                                      {"arguments",
                                       {{"asset", "ac100000-0000-4000-8000-000000000001"},
                                        {"transaction", token.Value().ToString()}}}},
                                     Janus::MCP::McpProtocolEra::Modern2026);
    REQUIRE(std::holds_alternative<Janus::MCP::Json>(response));
    auto result = std::get<Janus::MCP::Json>(response);
    REQUIRE_FALSE(result.value("isError", false));
    const auto id =
        Janus::UUID::Parse(result["structuredContent"]["entity"].get<std::string>()).Value();
    REQUIRE(project.FinishAuthoringTransaction(token.Value(), owner, true));
    REQUIRE(project.SaveCurrentScene());
    REQUIRE(project.PlayRuntime(true));
    for (int i = 0; i < 90; ++i)
        REQUIRE(project.StepRuntime());
    CHECK(project.GetRuntimeSession()->GetPhysics().GetBodyCount() == 4);
    REQUIRE(project.StopRuntime());
    Janus::Editor::EditorContext editorContext;
    editorContext.project = &project;
    Janus::Editor::EditorActions human(editorContext);
    REQUIRE(human.Undo());
    CHECK(project.GetEditorScene().GetEntities().size() == count);
    CHECK_FALSE(project.GetEditorScene().FindEntity(id).IsValid());
    REQUIRE(human.Redo());
    CHECK(project.GetEditorScene().FindEntity(id).IsValid());
    CHECK(project.GetEditorScene().GetEntities().size() == count + 2);
    REQUIRE(project.SaveCurrentScene());
    auto reopened = Janus::Editor::ProjectSession::Open(config, *renderer);
    REQUIRE(reopened);
    CHECK(reopened.Value()->GetEditorScene().FindEntity(id).IsValid());
}

TEST_CASE("Integrated game completes both outcomes and isolates neutral stepping from input",
          "[acceptance][game]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);
    Janus::ProjectRuntimeConfig config;
    config.root = std::filesystem::path(JANUS_TEST_SOURCE_DIR).parent_path() / "Game";
    auto opened = Janus::Editor::ProjectSession::Open(config, *renderer);
    REQUIRE(opened);
    auto project = std::move(opened).Value();
    auto authoring = Janus::SceneSerializer::Serialize(project->GetEditorScene(),
                                                       project->GetReflectionRegistry());
    REQUIRE(authoring);
    Janus::InputState input;
    REQUIRE(project->StartRuntime(input));
    auto fields = [&] { return project->GetRuntimeSession()->GetSnapshot()->fields; };
    auto tick = [&]
    {
        auto updated = project->UpdateRuntime(Janus::TimeStep::FromSeconds(1.0 / 60.0));
        INFO((updated ? "success" : updated.GetError().message));
        REQUIRE(updated);
    };
    auto key = [&](Janus::KeyCode code)
    {
        input.BeginFrame();
        input.Apply(Janus::KeyPressedEvent{code, false});
        input.Apply(Janus::KeyReleasedEvent{code});
        tick();
    };
    for (int i = 0; i < 90; ++i)
    {
        input.BeginFrame();
        tick();
    }
    CHECK(std::get<double>(fields().at("landings")) >= 2);
    CHECK(project->GetRuntimeSession()->GetPhysics().GetBodyCount() == 3);
    key(Janus::KeyCode::Digit1);
    for (int i = 0; i < 3; ++i)
    {
        key(Janus::KeyCode::Digit2);
        key(Janus::KeyCode::Digit4);
        CHECK(std::get<double>(fields().at("enemyHp")) == 8 - i * 4);
    }
    CHECK(std::get<std::string>(fields().at("phase")) == "victory");
    CHECK(std::get<double>(fields().at("hits")) == 5);
    REQUIRE(project->PauseRuntime());
    auto paused = project->GetRuntimeSession()->GetSnapshot();
    REQUIRE(project->UpdateRuntime(Janus::TimeStep::FromSeconds(10)));
    CHECK(project->GetRuntimeSession()->GetSnapshot()->fields == paused->fields);
    input.BeginFrame();
    input.Apply(Janus::KeyPressedEvent{Janus::KeyCode::Digit0, false});
    input.Apply(Janus::KeyReleasedEvent{Janus::KeyCode::Digit0});
    REQUIRE(project->StepRuntime());
    CHECK(std::get<std::string>(fields().at("phase")) == "victory");
    // Pause retains the device; output availability is not a playback/suspension flag.
    CHECK(std::get<double>(fields().at("musicAudioCursor")) >
          std::get<double>(paused->fields.at("musicAudioCursor")));
    input.BeginFrame();
    REQUIRE(project->ResumeRuntime());
    key(Janus::KeyCode::Digit0);
    CHECK(std::get<double>(fields().at("hits")) == 0);
    key(Janus::KeyCode::Digit1);
    for (int i = 0; i < 3; ++i)
    {
        key(Janus::KeyCode::Digit3);
        key(Janus::KeyCode::Digit4);
    }
    CHECK(std::get<std::string>(fields().at("phase")) == "defeat");
    CHECK(std::get<double>(fields().at("enemyHp")) == 12);
    CHECK_FALSE(project->IsDirty());
    CHECK(project->GetCommandBus().GetHistorySize() == 0);
    REQUIRE(project->StopRuntime());
    auto after = Janus::SceneSerializer::Serialize(project->GetEditorScene(),
                                                   project->GetReflectionRegistry());
    REQUIRE(after);
    CHECK(after.Value() == authoring.Value());
}
