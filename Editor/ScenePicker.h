#pragma once

#include "Core/Math/Vector2.h"
#include "Core/UUID/UUID.h"
#include "Renderer/RendererTypes.h"

#include <optional>

namespace Janus
{

class Scene;

namespace Editor
{

class EditorCamera;

[[nodiscard]] std::optional<UUID> PickSpriteEntity(
    Scene& scene,
    Vector2 worldPoint);

[[nodiscard]] std::optional<UUID> PickSpriteEntity(
    Scene& scene,
    const EditorCamera& camera,
    Viewport viewport,
    Vector2 viewportPoint);

} // namespace Editor
} // namespace Janus
