#pragma once
#include "Project/ProjectSettings.h"
#include <array>

namespace Janus::Editor
{
class ProjectSession;
class ProjectSettingsPanel final
{
  public:
    void Draw(ProjectSession& session);
    ProjectSettings GetDraft() const;
    bool HasUnsavedChanges(const ProjectSession& session) const;
    void AcceptSaved(const ProjectSettings& settings)
    {
        Reset(settings);
    }

  private:
    void Reset(const ProjectSettings& settings);
    bool m_Initialized = false;
    ProjectSettings m_Draft;
    std::array<char, 129> m_Name{};
    std::array<std::array<char, 1025>, 4> m_Paths{};
    std::array<char, 65> m_NewAction{};
    std::string m_Message;
};
} // namespace Janus::Editor
