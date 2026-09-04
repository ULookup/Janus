#include "EditorApplication.h"

#include "Application/Application.h"
#include "Application/ApplicationConfig.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <utility>

namespace
{

std::filesystem::path ResolveProjectRoot(int argc, char** argv)
{
    if (argc > 1 && argv[1] != nullptr)
    {
        return std::filesystem::path(argv[1]);
    }

    std::error_code error;
    if (argc > 0 && argv[0] != nullptr)
    {
        const std::filesystem::path executable =
            std::filesystem::absolute(argv[0], error);
        if (!error)
        {
            return executable.parent_path() / "SandboxProject";
        }
    }

    error.clear();
    const std::filesystem::path current =
        std::filesystem::current_path(error);
    return error
        ? std::filesystem::path("SandboxProject")
        : current / "SandboxProject";
}

} // namespace

int main(int argc, char** argv)
{
    Janus::ApplicationConfig config;
    config.window.title = "Janus Editor";
    config.window.width = 1440;
    config.window.height = 900;
    config.window.resizable = true;
    config.executionMode =
        Janus::ApplicationExecutionMode::ClientDriven;

    Janus::Application application(config);
    Janus::Editor::EditorApplication editor(
        ResolveProjectRoot(argc, argv));

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
