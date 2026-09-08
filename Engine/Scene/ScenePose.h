#pragma once

#include "Core/Error/Result.h"
#include "Core/Math/Mat4.h"
#include "Core/UUID/UUID.h"

#include <optional>

namespace Janus
{

class Scene;

struct ScenePositionOverride
{
    UUID entity;
    Vector2 position;
};

struct SceneWorldPose
{
    Mat4 matrix;
    Vector2 position;
    f32 rotationRadians = 0;
    Vector2 scale{1, 1};
};

class ScenePose
{
  public:
    // Resolve current local fields; authoring and cached world fields remain untouched.
    [[nodiscard]] static Result<SceneWorldPose>
    Resolve(const Scene& scene, UUID entity,
            std::optional<ScenePositionOverride> preview = std::nullopt);
};

} // namespace Janus
