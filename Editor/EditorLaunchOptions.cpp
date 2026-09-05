#include "EditorLaunchOptions.h"

#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace Janus::Editor
{
namespace
{

std::filesystem::path DefaultProjectRoot(
    int argc,
    char* const* argv)
{
    std::error_code error;

    if (argc > 0
        && argv != nullptr
        && argv[0] != nullptr)
    {
        const std::filesystem::path executable =
            std::filesystem::absolute(
                argv[0],
                error);

        if (!error)
        {
            return executable.parent_path()
                / "SandboxProject";
        }
    }

    error.clear();
    const std::filesystem::path current =
        std::filesystem::current_path(
            error);

    return error
        ? std::filesystem::path{
              "SandboxProject"}
        : current / "SandboxProject";
}

} // namespace

Result<EditorLaunchOptions> ParseEditorLaunchOptions(
    int argc,
    char* const* argv)
{
    if (argc < 0
        || (argc > 0 && argv == nullptr))
    {
        return Result<EditorLaunchOptions>::Failure(
            ErrorCode::InvalidArgument,
            "Invalid JanusEditor argument vector.");
    }

    EditorLaunchOptions options;
    std::optional<std::filesystem::path> explicitProject;
    std::optional<std::filesystem::path> positionalProject;

    for (int index = 1;
         index < argc;
         ++index)
    {
        if (argv[index] == nullptr)
        {
            return Result<EditorLaunchOptions>::Failure(
                ErrorCode::InvalidArgument,
                "JanusEditor argument cannot be null.");
        }

        const std::string_view argument{
            argv[index]};

        if (argument == "--mcp-stdio")
        {
            if (options.mcpStdio)
            {
                return Result<EditorLaunchOptions>::Failure(
                    ErrorCode::InvalidArgument,
                    "JanusEditor --mcp-stdio may only be specified once.");
            }

            options.mcpStdio = true;
            continue;
        }

        if (argument == "--project")
        {
            if (explicitProject.has_value())
            {
                return Result<EditorLaunchOptions>::Failure(
                    ErrorCode::InvalidArgument,
                    "JanusEditor --project may only be specified once.");
            }

            if (index + 1 >= argc
                || argv[index + 1] == nullptr)
            {
                return Result<EditorLaunchOptions>::Failure(
                    ErrorCode::InvalidArgument,
                    "JanusEditor --project requires a path.");
            }

            const std::string_view path{
                argv[++index]};

            if (path.empty())
            {
                return Result<EditorLaunchOptions>::Failure(
                    ErrorCode::InvalidArgument,
                    "JanusEditor project path cannot be empty.");
            }

            explicitProject =
                std::filesystem::path{
                    std::string{path}};
            continue;
        }

        if (argument.starts_with("--"))
        {
            return Result<EditorLaunchOptions>::Failure(
                ErrorCode::InvalidArgument,
                "Unknown JanusEditor option '"
                    + std::string{argument}
                    + "'.");
        }

        if (positionalProject.has_value())
        {
            return Result<EditorLaunchOptions>::Failure(
                ErrorCode::InvalidArgument,
                "JanusEditor accepts at most one legacy positional project path.");
        }

        if (argument.empty())
        {
            return Result<EditorLaunchOptions>::Failure(
                ErrorCode::InvalidArgument,
                "JanusEditor project path cannot be empty.");
        }

        positionalProject =
            std::filesystem::path{
                std::string{argument}};
    }

    if (explicitProject.has_value()
        && positionalProject.has_value())
    {
        return Result<EditorLaunchOptions>::Failure(
            ErrorCode::InvalidArgument,
            "Use either --project or the legacy positional project path, not both.");
    }

    options.projectRoot =
        explicitProject.has_value()
            ? std::move(*explicitProject)
            : positionalProject.has_value()
                ? std::move(*positionalProject)
                : DefaultProjectRoot(
                      argc,
                      argv);

    return Result<EditorLaunchOptions>::Success(
        std::move(options));
}

} // namespace Janus::Editor
