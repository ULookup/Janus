#pragma once

#include "Core/Error/Result.h"

#include <filesystem>
#include <memory>
#include <string_view>

namespace Janus
{

class Scene;

class SceneDeserializer
{
public:
    [[nodiscard]] static Result<std::unique_ptr<Scene>> Deserialize(
        std::string_view text);
    [[nodiscard]] static Result<std::unique_ptr<Scene>> Load(
        const std::filesystem::path& path);
};

} // namespace Janus
