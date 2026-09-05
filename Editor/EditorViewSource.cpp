#include "EditorViewSource.h"

#include "ProjectSession.h"
#include "RuntimeSession.h"

#include "Scene/Scene.h"

namespace Janus::Editor
{

Scene& ResolveSceneViewScene(
    ProjectSession& project) noexcept
{
    return project.GetEditorScene();
}

Scene& ResolveGameViewScene(
    ProjectSession& project) noexcept
{
    if (auto* runtime = project.GetRuntimeSession();
        runtime != nullptr)
    {
        return runtime->GetScene();
    }

    return project.GetEditorScene();
}

} // namespace Janus::Editor
