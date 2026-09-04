#pragma once

#include "Core/Error/Result.h"

#include <memory>

namespace Janus
{

class Scene;

class SceneCloner final
{
public:
    SceneCloner() = delete;

    [[nodiscard]] static Result<std::unique_ptr<Scene>> Clone(
        const Scene& source);
};

} // namespace Janus
