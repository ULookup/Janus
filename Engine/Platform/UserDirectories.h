#pragma once

#include "Core/Error/Result.h"
#include <filesystem>

namespace Janus::Platform
{
// The platform may create the application's per-user writable directory.
[[nodiscard]] Result<std::filesystem::path> GetUserDataDirectory();
// Read-only location; fonts are discovered in place and never copied into the project.
[[nodiscard]] Result<std::filesystem::path> GetSystemFontDirectory();
} // namespace Janus::Platform
