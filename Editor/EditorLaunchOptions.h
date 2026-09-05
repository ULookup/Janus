#pragma once

#include "Core/Error/Result.h"

#include <filesystem>

namespace Janus::Editor
{

struct EditorLaunchOptions
{
    std::filesystem::path projectRoot;
    bool mcpStdio = false;
};

[[nodiscard]] Result<EditorLaunchOptions> ParseEditorLaunchOptions(
    int argc,
    char* const* argv);

} // namespace Janus::Editor
