#include "../Asset/AssetTestUtils.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Input/InputActions.h"
#include "Core/Input/InputState.h"
#include "Project/ProjectSettings.h"
#include <catch2/catch_test_macros.hpp>

using namespace Janus;

TEST_CASE("Project settings preserve legacy defaults and round trip bindings",
          "[v0.10][project-settings]")
{
    Test::AssetTempDirectory temp;
    auto loaded = LoadProjectSettings({temp.Path()});
    REQUIRE(loaded);
    REQUIRE(loaded.Value().defaultScene == "Scenes/Battle.scene");
    REQUIRE(loaded.Value().inputBindings.empty());
    auto settings = loaded.Value();
    settings.name = "Input Demo";
    settings.width = 960;
    settings.height = 540;
    settings.vsync = false;
    settings.targetFps = 120;
    settings.inputBindings["MoveLeft"] = {KeyCode::ArrowLeft, KeyCode::A};
    REQUIRE(SaveProjectSettings(temp.Path(), settings));
    auto reopened = LoadProjectSettings({temp.Path()});
    REQUIRE(reopened);
    REQUIRE(reopened.Value() == settings);
    REQUIRE_FALSE(FileSystem::Exists(temp.Path() / "Scenes/Battle.scene"));
}

TEST_CASE("Project settings reject invalid documents without replacing valid settings",
          "[v0.10][project-settings]")
{
    Test::AssetTempDirectory temp;
    ProjectSettings settings;
    REQUIRE(SaveProjectSettings(temp.Path(), settings));
    const auto original = FileSystem::ReadText(temp.Path() / "project.json").Value();
    settings.width = 0;
    REQUIRE_FALSE(SaveProjectSettings(temp.Path(), settings));
    REQUIRE(FileSystem::ReadText(temp.Path() / "project.json").Value() == original);
    for (const auto text : {"[]", "{", "{\"version\":99}", "{\"version\":1,\"version\":1}",
                            "{\"version\":1,\"width\":-1}", "{\"version\":1,\"vsync\":1}",
                            "{\"version\":1,\"unknown\":true}",
                            "{\"version\":1,\"defaultScene\":\"../outside.scene\"}",
                            "{\"version\":1,\"inputBindings\":{\"Confirm\":[\"Typo\"]}}"})
    {
        INFO(text);
        REQUIRE(FileSystem::WriteText(temp.Path() / "project.json", text));
        REQUIRE_FALSE(LoadProjectSettings({temp.Path()}));
    }
}

TEST_CASE("Project settings preserve UTF8 and reject invalid UTF8 without throwing",
          "[v0.10][project-settings]")
{
    Test::AssetTempDirectory temp;
    ProjectSettings settings;
    settings.name = "测试项目";
    settings.defaultScene = std::filesystem::path(u8"Scenes/测试.scene");
    REQUIRE(SaveProjectSettings(temp.Path(), settings));
    REQUIRE(LoadProjectSettings({temp.Path()}).Value() == settings);
    settings.name = std::string(1, static_cast<char>(0xff));
    REQUIRE_FALSE(SaveProjectSettings(temp.Path(), settings));
}

TEST_CASE("Project configuration overrides and invalid paths are explicit",
          "[v0.10][project-settings]")
{
    Test::AssetTempDirectory temp;
    ProjectSettings settings;
    settings.defaultScene = "Scenes/Other.scene";
    REQUIRE(SaveProjectSettings(temp.Path(), settings));
    ProjectRuntimeConfig config{temp.Path()};
    REQUIRE(LoadProjectSettings(config).Value().defaultScene == "Scenes/Other.scene");
    config.startupScenePath = "Scenes/Explicit.scene";
    REQUIRE(LoadProjectSettings(config).Value().defaultScene == "Scenes/Explicit.scene");
    for (const auto path : {"../outside", "C:/outside", ".", "Assets/file:stream", ""})
        REQUIRE_FALSE(ResolveProjectPath(temp.Path(), path));
    REQUIRE(ResolveProjectPath(temp.Path(), "Assets/../Scripts/test.lua"));
}

TEST_CASE("Action input aggregates multiple keys and preserves short taps",
          "[v0.10][input-actions]")
{
    InputBindings bindings{{"Confirm", {KeyCode::Space, KeyCode::Enter}}};
    REQUIRE(ValidateInputBindings(bindings));
    InputState input;
    input.BeginFrame();
    input.Apply(KeyPressedEvent{KeyCode::Space, false});
    REQUIRE(QueryInputAction(bindings, input, "Confirm")->pressed);
    input.BeginFrame();
    input.Apply(KeyPressedEvent{KeyCode::Enter, false});
    input.Apply(KeyReleasedEvent{KeyCode::Space});
    REQUIRE(QueryInputAction(bindings, input, "Confirm")->down);
    REQUIRE_FALSE(QueryInputAction(bindings, input, "Confirm")->pressed);
    REQUIRE_FALSE(QueryInputAction(bindings, input, "Confirm")->released);
    input.BeginFrame();
    input.Apply(WindowFocusLostEvent{});
    REQUIRE(QueryInputAction(bindings, input, "Confirm")->released);
    REQUIRE_FALSE(QueryInputAction(bindings, input, "Confirm")->down);
    input.BeginFrame();
    input.Apply(KeyPressedEvent{KeyCode::Space, false});
    input.Apply(KeyReleasedEvent{KeyCode::Space});
    auto tapped = QueryInputAction(bindings, input, "Confirm");
    REQUIRE(tapped->pressed);
    REQUIRE(tapped->released);
    REQUIRE_FALSE(tapped->down);
    REQUIRE_FALSE(QueryInputAction(bindings, input, "Typo"));
    REQUIRE_FALSE(ValidateInputBindings({{"", {KeyCode::A}}}));
    REQUIRE_FALSE(ValidateInputBindings({{"Confirm", {KeyCode::Count}}}));
    REQUIRE_FALSE(ValidateInputBindings({{"Confirm", {KeyCode::A, KeyCode::A}}}));
}

TEST_CASE("Repeated input and invalid pointer data do not fabricate action presses",
          "[v0.10][input-actions]")
{
    const InputBindings bindings{{"Confirm", {KeyCode::Space}}};
    InputState input;
    input.Apply(KeyPressedEvent{KeyCode::Space, true});
    REQUIRE(QueryInputAction(bindings, input, "Confirm")->down);
    REQUIRE_FALSE(QueryInputAction(bindings, input, "Confirm")->pressed);
    input.BeginFrame();
    input.Apply(KeyPressedEvent{KeyCode::Space, true});
    REQUIRE_FALSE(QueryInputAction(bindings, input, "Confirm")->pressed);
    input.Apply(PointerButtonPressedEvent{PointerButton::Count});
    REQUIRE_FALSE(input.IsPointerButtonDown(PointerButton::Count));
    REQUIRE_FALSE(input.ForViewport({}, {}, {800, 400}, true).GetPointerPosition());
    input = {};
    input.Apply(KeyReleasedEvent{KeyCode::Space});
    REQUIRE_FALSE(QueryInputAction(bindings, input, "Confirm")->released);
    input.Apply(KeyPressedEvent{KeyCode::Space, false});
    input.Apply(WindowFocusLostEvent{});
    REQUIRE_FALSE(QueryInputAction(bindings, input, "Confirm")->pressed);
}

TEST_CASE("Game viewport input maps pointer and excludes panels and letterbox",
          "[v0.10][input-actions]")
{
    InputState input;
    input.Apply(PointerMovedEvent{{150, 100}});
    input.Apply(PointerButtonPressedEvent{PointerButton::Left});
    input.Apply(KeyPressedEvent{KeyCode::A, false});
    auto mapped = input.ForViewport({100, 50}, {200, 100}, {800, 400}, true);
    REQUIRE(mapped.GetPointerPosition()->x == 200);
    REQUIRE(mapped.GetPointerPosition()->y == 200);
    REQUIRE(mapped.IsPointerButtonDown(PointerButton::Left));
    REQUIRE(mapped.IsKeyDown(KeyCode::A));
    auto blocked = input.ForViewport({200, 50}, {200, 100}, {800, 400}, true);
    REQUIRE_FALSE(blocked.GetPointerPosition());
    REQUIRE_FALSE(blocked.IsPointerButtonDown(PointerButton::Left));
    REQUIRE_FALSE(blocked.IsKeyDown(KeyCode::A));
    REQUIRE_FALSE(
        input.ForViewport({100, 50}, {200, 100}, {800, 400}, false).IsKeyDown(KeyCode::A));
    input.Apply(WindowFocusLostEvent{});
    REQUIRE_FALSE(input.IsPointerButtonDown(PointerButton::Left));
    REQUIRE(input.WasPointerButtonReleased(PointerButton::Left));
    REQUIRE_FALSE(input.GetPointerPosition());
}
