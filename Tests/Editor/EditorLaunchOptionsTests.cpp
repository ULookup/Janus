#include "EditorLaunchOptions.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

TEST_CASE(
    "JanusEditor launch options preserve legacy positional project path",
    "[editor][launch][v0.8]")
{
    char executable[] = "JanusEditor.exe";
    char project[] = "MyProject";
    char* argv[] = {
        executable,
        project};

    const auto parsed =
        Janus::Editor::ParseEditorLaunchOptions(
            2,
            argv);

    REQUIRE(parsed);
    REQUIRE(
        parsed.Value().projectRoot
        == std::filesystem::path{"MyProject"});
    REQUIRE_FALSE(parsed.Value().mcpStdio);
}

TEST_CASE(
    "JanusEditor launch options support explicit project and MCP stdio mode",
    "[editor][launch][v0.8]")
{
    char executable[] = "JanusEditor.exe";
    char projectFlag[] = "--project";
    char project[] = "SandboxProject";
    char mcpFlag[] = "--mcp-stdio";

    char* argv[] = {
        executable,
        projectFlag,
        project,
        mcpFlag};

    const auto parsed =
        Janus::Editor::ParseEditorLaunchOptions(
            4,
            argv);

    REQUIRE(parsed);
    REQUIRE(
        parsed.Value().projectRoot
        == std::filesystem::path{
            "SandboxProject"});
    REQUIRE(parsed.Value().mcpStdio);
}

TEST_CASE(
    "JanusEditor launch options reject ambiguous or unknown arguments",
    "[editor][launch][v0.8]")
{
    SECTION("explicit and positional project conflict")
    {
        char executable[] = "JanusEditor.exe";
        char projectFlag[] = "--project";
        char explicitProject[] = "One";
        char positionalProject[] = "Two";

        char* argv[] = {
            executable,
            projectFlag,
            explicitProject,
            positionalProject};

        const auto parsed =
            Janus::Editor::ParseEditorLaunchOptions(
                4,
                argv);

        REQUIRE_FALSE(parsed);
    }

    SECTION("unknown option")
    {
        char executable[] = "JanusEditor.exe";
        char unknown[] = "--unknown";

        char* argv[] = {
            executable,
            unknown};

        const auto parsed =
            Janus::Editor::ParseEditorLaunchOptions(
                2,
                argv);

        REQUIRE_FALSE(parsed);
    }

    SECTION("missing project value")
    {
        char executable[] = "JanusEditor.exe";
        char projectFlag[] = "--project";

        char* argv[] = {
            executable,
            projectFlag};

        const auto parsed =
            Janus::Editor::ParseEditorLaunchOptions(
                2,
                argv);

        REQUIRE_FALSE(parsed);
    }
}

TEST_CASE(
    "JanusEditor launch options derive SandboxProject next to executable",
    "[editor][launch][v0.8]")
{
    char executable[] =
        "C:/Janus/bin/JanusEditor.exe";

    char* argv[] = {
        executable};

    const auto parsed =
        Janus::Editor::ParseEditorLaunchOptions(
            1,
            argv);

    REQUIRE(parsed);
    REQUIRE(
        parsed.Value().projectRoot.filename()
        == "SandboxProject");
    REQUIRE(
        parsed.Value().projectRoot.parent_path().filename()
        == "bin");
}
