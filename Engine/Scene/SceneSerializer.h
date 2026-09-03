#pragma once

#include "Core/Error/Result.h"

#include <filesystem>
#include <string>

namespace Janus
{

class Scene;

class SceneSerializer
{
public:
    [[nodiscard]] static Result<std::string> Serialize(const Scene& scene);
    [[nodiscard]] static Result<void> Save(
        const Scene& scene,
        const std::filesystem::path& path);
};

} // namespace Janus
