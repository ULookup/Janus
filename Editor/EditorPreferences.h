#pragma once

#include "Core/Error/Result.h"
#include "Core/Math/Vector2.h"
#include "EditorLocale.h"
#include "EditorWorkspaceLayout.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Janus::Editor
{
// Resolve once when opening a project. An explicit base also makes relative-key tests independent
// of process cwd; an empty base uses the current working directory.
[[nodiscard]] Result<std::filesystem::path>
ResolveEditorPreferenceProjectKey(const std::filesystem::path& projectRoot,
                                  const std::filesystem::path& baseDirectory = {});

struct EditorCameraPreference
{
    std::filesystem::path projectRoot;
    std::filesystem::path scenePath;
    Vector2 position;
    f32 zoom = 1;
};

// Local user state only; no device DPI, entity handles or authoring data is persisted.
struct EditorPreferences
{
    f32 userScale = 1;
    EditorLanguage language = EditorLanguage::English;
    EditorWorkspacePreferences workspace;
    Vector2 workspaceReferenceSize;
    bool showGrid = true;
    std::map<std::string, bool> panelExpanded;
    std::vector<std::filesystem::path> recentProjects;
    std::vector<EditorCameraPreference> cameras;
    bool wasClamped = false;

    static Result<EditorPreferences> Load(const std::filesystem::path& path);
    Result<void> Save(const std::filesystem::path& path) const;
    Result<std::string> Serialize() const;
    Result<void> Normalize();
    Result<void> RememberProject(const std::filesystem::path& projectRoot);
    Result<void> RememberCamera(EditorCameraPreference camera);
    const EditorCameraPreference* FindCamera(const std::filesystem::path& projectRoot,
                                             const std::filesystem::path& scenePath) const;
};
} // namespace Janus::Editor
