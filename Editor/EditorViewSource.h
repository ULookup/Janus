#pragma once

namespace Janus
{

class Scene;

namespace Editor
{

class ProjectSession;

[[nodiscard]] Scene& ResolveSceneViewScene(
    ProjectSession& project) noexcept;

[[nodiscard]] Scene& ResolveGameViewScene(
    ProjectSession& project) noexcept;

} // namespace Editor
} // namespace Janus
