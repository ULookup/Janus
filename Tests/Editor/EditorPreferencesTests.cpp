#include "Core/FileSystem/FileSystem.h"
#include "Core/UUID/UUID.h"
#include "EditorPreferences.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <limits>
#include <nlohmann/json.hpp>

namespace
{
using namespace Janus;
using namespace Janus::Editor;
struct PreferencesFixture
{
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("janus-preferences-" + UUID::Random().ToString());
    std::filesystem::path path = root / "editor-preferences.json";
    PreferencesFixture()
    {
        std::filesystem::create_directories(root);
    }
    ~PreferencesFixture()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
};
} // namespace

TEST_CASE("Editor preferences missing file defaults and round trip remain outside project data",
          "[editor][preferences]")
{
    PreferencesFixture f;
    auto missing = EditorPreferences::Load(f.path);
    REQUIRE(missing);
    CHECK(missing.Value().userScale == 1);
    CHECK(missing.Value().language == EditorLanguage::English);
    CHECK(missing.Value().workspace.mode == EditorLayoutMode::Standard);
    CHECK(missing.Value().showGrid);
    CHECK_FALSE(std::filesystem::exists(f.path));
    auto preferences = missing.Value();
    preferences.userScale = 1.5f;
    preferences.language = EditorLanguage::Chinese;
    preferences.workspace = GetDefaultWorkspacePreferences(EditorLayoutMode::Debug);
    preferences.workspace.leftWidth = 275;
    preferences.workspaceReferenceSize = {1440, 900};
    preferences.showGrid = false;
    preferences.panelExpanded["Transform"] = false;
    preferences.panelExpanded["SpriteRenderer"] = true;
    const std::filesystem::path project = f.root / std::filesystem::path(u8"中文项目");
    REQUIRE(preferences.RememberProject(project));
    REQUIRE(preferences.RememberCamera({project, "Scenes/Battle.scene", {12, -45}, 0.25f}));
    REQUIRE(preferences.Save(f.path));
    auto loaded = EditorPreferences::Load(f.path);
    REQUIRE(loaded);
    CHECK(loaded.Value().userScale == 1.5f);
    CHECK(loaded.Value().language == EditorLanguage::Chinese);
    CHECK(loaded.Value().workspace.mode == EditorLayoutMode::Debug);
    CHECK(loaded.Value().workspace.leftWidth == 275);
    CHECK(loaded.Value().workspaceReferenceSize.x == 1440);
    CHECK(loaded.Value().workspaceReferenceSize.y == 900);
    CHECK_FALSE(loaded.Value().showGrid);
    CHECK_FALSE(loaded.Value().panelExpanded.at("Transform"));
    REQUIRE(loaded.Value().recentProjects.size() == 1);
    CHECK(loaded.Value().recentProjects[0] == project);
    const auto* camera = loaded.Value().FindCamera(project, "Scenes/Battle.scene");
    REQUIRE(camera);
    CHECK(camera->position.x == 12);
    CHECK(camera->position.y == -45);
    CHECK(camera->zoom == 0.25f);
    CHECK_FALSE(loaded.Value().FindCamera(project, "Scenes/Other.scene"));
    CHECK_FALSE(std::filesystem::exists(project));
    auto serialized = preferences.Serialize();
    REQUIRE(serialized);
    auto bytes = FileSystem::ReadText(f.path);
    REQUIRE(bytes);
    CHECK(serialized.Value() == bytes.Value());
}

TEST_CASE("Editor preferences reject malformed oversized and incorrectly typed files",
          "[editor][preferences]")
{
    PreferencesFixture f;
    std::string text;
    SECTION("truncated")
    {
        text = "{\"version\":1,";
    }
    SECTION("wrong version")
    {
        text = R"({"version":2})";
    }
    SECTION("wrong root")
    {
        text = "[]";
    }
    SECTION("boolean version")
    {
        text = R"({"version":true})";
    }
    SECTION("missing version")
    {
        text = "{}";
    }
    SECTION("string scale")
    {
        text = R"({"version":1,"userScale":"1.5"})";
    }
    SECTION("unknown language")
    {
        text = R"({"version":1,"language":"ar"})";
    }
    SECTION("unknown mode")
    {
        text = R"({"version":1,"workspace":{"mode":"unknown"}})";
    }
    SECTION("wrong panel value")
    {
        text = R"({"version":1,"panelExpanded":{"Transform":1}})";
    }
    SECTION("wrong grid")
    {
        text = R"({"version":1,"showGrid":1})";
    }
    SECTION("partial reference size")
    {
        text = R"({"version":1,"workspaceReferenceSize":{"x":0,"y":900}})";
    }
    SECTION("nonfinite exponent")
    {
        text = R"({"version":1,"userScale":1e999})";
    }
    SECTION("wrong camera position")
    {
        text =
            R"({"version":1,"cameras":[{"projectRoot":"project","scenePath":"scene","position":[0,0],"zoom":1}]})";
    }
    SECTION("too many recent projects")
    {
        nlohmann::json json{{"version", 1}, {"recentProjects", std::vector<std::string>(11, "x")}};
        text = json.dump();
    }
    SECTION("oversized")
    {
        text = std::string(65537, ' ');
    }
    SECTION("deep nesting")
    {
        text = "{\"version\":1,\"unknown\":" + std::string(1000, '[') + "0" +
               std::string(1000, ']') + "}";
    }
    REQUIRE(FileSystem::WriteText(f.path, text));
    CHECK_FALSE(EditorPreferences::Load(f.path));
}

TEST_CASE("Editor preferences clamp finite dimensions without persisting device DPI",
          "[editor][preferences]")
{
    PreferencesFixture f;
    REQUIRE(FileSystem::WriteText(f.path, R"({"version":1,"userScale":999,
        "workspace":{"mode":"focus","leftWidth":-100,"rightWidth":999,"utilityHeight":900},
        "cameras":[{"projectRoot":"project","scenePath":"scene","position":{"x":1e30,"y":-1e30},"zoom":0}]})"));
    auto loaded = EditorPreferences::Load(f.path);
    REQUIRE(loaded);
    CHECK(loaded.Value().wasClamped);
    CHECK(loaded.Value().userScale == 2);
    CHECK(loaded.Value().workspace.mode == EditorLayoutMode::Focus);
    CHECK(loaded.Value().workspace.leftWidth == 170);
    CHECK(loaded.Value().workspace.rightWidth == 520);
    CHECK(loaded.Value().workspace.utilityHeight == 600);
    REQUIRE(loaded.Value().cameras.size() == 1);
    CHECK(loaded.Value().cameras[0].position.x == 1e7f);
    CHECK(loaded.Value().cameras[0].position.y == -1e7f);
    CHECK(loaded.Value().cameras[0].zoom == 0.001f);
    REQUIRE(loaded.Value().Save(f.path));
    auto reloaded = EditorPreferences::Load(f.path);
    REQUIRE(reloaded);
    CHECK_FALSE(reloaded.Value().wasClamped);
    auto text = FileSystem::ReadText(f.path);
    REQUIRE(text);
    CHECK(text.Value().find("dpi") == std::string::npos);
    CHECK(text.Value().find("wasClamped") == std::string::npos);
}

TEST_CASE("Editor preferences retain bounded recent projects and camera views by stable paths",
          "[editor][preferences]")
{
    EditorPreferences preferences;
    for (int i = 0; i < 40; ++i)
    {
        const auto project = std::filesystem::path("Projects") / std::to_string(i);
        REQUIRE(preferences.RememberProject(project));
        REQUIRE(preferences.RememberCamera(
            {project, "Scenes/Main.scene", {static_cast<float>(i), 2}, 1}));
    }
    CHECK(preferences.recentProjects.size() == 10);
    CHECK(preferences.cameras.size() == 32);
    CHECK(preferences.recentProjects.front() == std::filesystem::path("Projects/39"));
    REQUIRE(preferences.RememberProject("Projects/./35"));
    CHECK(preferences.recentProjects.size() == 10);
    CHECK(preferences.recentProjects.front() == std::filesystem::path("Projects/35"));
    REQUIRE(preferences.RememberCamera({"Projects/35", "Scenes/./Main.scene", {7, 8}, 20}));
    CHECK(preferences.cameras.size() == 32);
    const auto* camera = preferences.FindCamera("Projects/./35", "Scenes/Main.scene");
    REQUIRE(camera);
    CHECK(camera->position.x == 7);
    CHECK(camera->zoom == 20);
    CHECK_FALSE(preferences.FindCamera("Projects/0", "Scenes/Main.scene"));
}

TEST_CASE("Editor preferences invalid writes preserve previous bytes and report IO failures",
          "[editor][preferences]")
{
    PreferencesFixture f;
    EditorPreferences preferences;
    REQUIRE(preferences.Save(f.path));
    auto before = FileSystem::ReadText(f.path);
    REQUIRE(before);
    SECTION("nonfinite state")
    {
        preferences.userScale = std::numeric_limits<float>::infinity();
    }
    SECTION("too many panels")
    {
        for (int i = 0; i < 65; ++i)
            preferences.panelExpanded[std::to_string(i)] = true;
    }
    SECTION("invalid panel UTF8")
    {
        preferences.panelExpanded["\xff"] = true;
    }
    SECTION("oversized path")
    {
        preferences.recentProjects.emplace_back(std::string(4097, 'x'));
    }
    SECTION("too many cameras")
    {
        preferences.cameras.resize(33);
    }
    CHECK_FALSE(preferences.Save(f.path));
    auto after = FileSystem::ReadText(f.path);
    REQUIRE(after);
    CHECK(after.Value() == before.Value());
    CHECK_FALSE(EditorPreferences{}.Save(f.path / "child.json"));
}

TEST_CASE("Editor preference serialized size limit is enforced before replacing valid data",
          "[editor][preferences]")
{
    PreferencesFixture f;
    EditorPreferences original;
    REQUIRE(original.Save(f.path));
    auto before = FileSystem::ReadText(f.path);
    REQUIRE(before);
    EditorPreferences oversized;
    for (int index = 0; index < 32; ++index)
        oversized.cameras.push_back(
            {std::string(4000, 'x') + std::to_string(index), "Scene", {}, 1});
    REQUIRE(oversized.Normalize());
    CHECK_FALSE(oversized.Serialize());
    CHECK_FALSE(oversized.Save(f.path));
    auto after = FileSystem::ReadText(f.path);
    REQUIRE(after);
    CHECK(after.Value() == before.Value());
}

TEST_CASE("Editor preference project keys distinguish working directories and unify path spellings",
          "[editor][preferences]")
{
    PreferencesFixture f;
    const auto firstDirectory = f.root / "First";
    const auto secondDirectory = f.root / "Second";
    std::filesystem::create_directories(firstDirectory / "Game");
    std::filesystem::create_directories(secondDirectory / "Game");
    const auto first = ResolveEditorPreferenceProjectKey("Game", firstDirectory);
    const auto second = ResolveEditorPreferenceProjectKey("Game", secondDirectory);
    const auto absolute = ResolveEditorPreferenceProjectKey(firstDirectory / "Game");
    const auto alternate = ResolveEditorPreferenceProjectKey("Game/../Game/.", firstDirectory);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(absolute);
    REQUIRE(alternate);
    CHECK(first.Value().is_absolute());
    CHECK(first.Value() != second.Value());
    CHECK(first.Value() == absolute.Value());
    CHECK(first.Value() == alternate.Value());
    EditorPreferences preferences;
    REQUIRE(preferences.RememberProject(first.Value()));
    REQUIRE(preferences.RememberProject(absolute.Value()));
    REQUIRE(preferences.RememberProject(second.Value()));
    CHECK(preferences.recentProjects.size() == 2);
    REQUIRE(preferences.RememberCamera({first.Value(), "Scenes/Main.scene", {12, 34}, 1}));
    CHECK(preferences.FindCamera(absolute.Value(), "Scenes/Main.scene"));
    CHECK_FALSE(preferences.FindCamera(second.Value(), "Scenes/Main.scene"));
    CHECK_FALSE(ResolveEditorPreferenceProjectKey({}, firstDirectory));
}
