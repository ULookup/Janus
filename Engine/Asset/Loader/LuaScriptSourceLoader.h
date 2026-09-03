#pragma once

#include "Core/Error/Result.h"

#include <filesystem>
#include <string>

namespace Janus
{

class LuaScriptSourceLoader
{
public:
    [[nodiscard]] static Result<std::string> Load(
        const std::filesystem::path& path);
};

} // namespace Janus
