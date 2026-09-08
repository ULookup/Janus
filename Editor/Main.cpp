#include "EditorApplication.h"
#include "EditorLaunchOptions.h"

#include "Application/Application.h"
#include "Application/ApplicationConfig.h"
#include "Project/ProjectSettings.h"

#include <cstdio>
#include <cstdlib>
#include <utility>

int main(int argc, char** argv)
{
    auto options =
        Janus::Editor::ParseEditorLaunchOptions(
            argc,
            argv);

    if (!options)
    {
        std::fprintf(
            stderr,
            "JanusEditor arguments invalid: %s\n",
            options.GetError().message.c_str());
        return EXIT_FAILURE;
    }

    Janus::Editor::EditorLaunchOptions launch =
        std::move(options).Value();

    auto settings = Janus::LoadProjectSettings({launch.projectRoot});
    if (!settings)
    {
        std::fprintf(stderr, "Project settings invalid: %s\n", settings.GetError().message.c_str());
        return EXIT_FAILURE;
    }
    Janus::ApplicationConfig config;
    config.vsync = settings.Value().vsync;
    config.targetFps = settings.Value().targetFps;
    config.window.title = launch.projectRoot.filename().string() + " - Janus Editor";
    config.window.width = 1440;
    config.window.height = 900;
    config.window.resizable = true;
    config.executionMode =
        Janus::ApplicationExecutionMode::ClientDriven;

    if (launch.mcpStdio)
    {
        config.logOutput =
            Janus::LogOutput::StandardError;
    }

    Janus::Application application(config);
    Janus::Editor::EditorApplication editor(
        std::move(launch.projectRoot),
        launch.mcpStdio);

    const auto result = application.Run(editor);
    if (!result)
    {
        std::fprintf(
            stderr,
            "JanusEditor failed: %s\n",
            result.GetError().message.c_str());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
