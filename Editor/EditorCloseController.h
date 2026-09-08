#pragma once

#include "Core/Error/Result.h"
#include "Core/UUID/UUID.h"
#include <optional>

namespace Janus
{
struct ProjectSettings;
namespace Editor
{
class ProjectSession;

// Session-owned state stays behind ProjectSession. This owner-thread controller only coordinates
// explicit Human decisions and must be destroyed before its borrowed session.
class EditorCloseController final
{
  public:
    explicit EditorCloseController(ProjectSession& session) noexcept : m_Session(session) {}
    ~EditorCloseController();
    EditorCloseController(const EditorCloseController&) = delete;
    EditorCloseController& operator=(const EditorCloseController&) = delete;

    void Request();
    void Cancel() noexcept;
    bool IsPending() const noexcept
    {
        return m_OwnsGuard && !m_Accepted;
    }
    bool IsAccepted() const noexcept
    {
        return m_Accepted;
    }
    const std::optional<Error>& GetError() const noexcept
    {
        return m_Error;
    }
    Result<void> RollbackTransaction();
    Result<void> Confirm(bool saveScene, bool stopRuntime, const ProjectSettings* saveSettings);

  private:
    Result<void> Failed(Error error);
    ProjectSession& m_Session;
    bool m_OwnsGuard = false;
    bool m_Accepted = false;
    UUID m_Transaction;
    UUID m_TransactionOwner;
    std::optional<Error> m_Error;
};
} // namespace Editor
} // namespace Janus
