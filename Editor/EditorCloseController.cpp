#include "EditorCloseController.h"
#include "ProjectSession.h"

namespace Janus::Editor
{
EditorCloseController::~EditorCloseController()
{
    if (m_OwnsGuard)
        m_Session.m_ClosePending = false;
}

void EditorCloseController::Request()
{
    if (m_OwnsGuard || m_Session.m_ClosePending)
        return;
    m_OwnsGuard = true;
    m_Session.m_ClosePending = true;
    m_Transaction = m_Session.GetCommandBus().GetTransactionId();
    m_TransactionOwner = m_Session.GetTransactionOwner();
    m_Error.reset();
}

void EditorCloseController::Cancel() noexcept
{
    if (!IsPending())
        return;
    m_Session.m_ClosePending = false;
    m_OwnsGuard = false;
    m_Error.reset();
}

Result<void> EditorCloseController::Failed(Error error)
{
    m_Error = std::move(error);
    return Result<void>::Failure(*m_Error);
}

Result<void> EditorCloseController::RollbackTransaction()
{
    if (!IsPending())
        return Failed({ErrorCode::InvalidState, "No close confirmation is active."});
    auto& commands = m_Session.GetCommandBus();
    if (!commands.HasTransaction())
        return Result<void>::Success();
    if (commands.GetTransactionId() != m_Transaction ||
        m_Session.GetTransactionOwner() != m_TransactionOwner)
        return Failed({ErrorCode::InvalidState, "Transaction changed; cancel and review again."});
    m_Session.CancelAuthoringTransaction(m_TransactionOwner, CommandActor::Human);
    if (commands.RecoveryRequired() || commands.HasTransaction())
        return Failed({ErrorCode::InvalidState, "Transaction rollback failed. Save is blocked; "
                                                "cancel or explicitly discard and exit."});
    m_Error.reset();
    return Result<void>::Success();
}

Result<void> EditorCloseController::Confirm(bool saveScene, bool stopRuntime,
                                            const ProjectSettings* saveSettings)
{
    if (!IsPending())
        return Failed({ErrorCode::InvalidState, "No close confirmation is active."});
    auto& commands = m_Session.GetCommandBus();
    if (commands.HasTransaction() && !commands.RecoveryRequired())
        return Failed({ErrorCode::InvalidState,
                       "Finish or explicitly roll back the Agent transaction first."});
    if (commands.RecoveryRequired() && (saveScene || saveSettings))
        return Failed({ErrorCode::InvalidState, "Authoring recovery required; saving is unsafe."});
    if (m_Session.HasRuntime() && !stopRuntime)
        return Failed({ErrorCode::InvalidState, "Confirm Stop runtime before exiting."});
    // Validate drafts before any irreversible stop or disk write.
    if (saveSettings)
    {
        auto valid = ValidateProjectSettings(m_Session.GetProjectRoot(), *saveSettings);
        if (!valid)
            return Failed(valid.GetError());
    }
    if (m_Session.HasRuntime())
    {
        auto stopped = m_Session.StopRuntimeImpl();
        if (!stopped)
            return Failed(stopped.GetError());
    }
    if (saveSettings)
    {
        auto saved = m_Session.SaveProjectSettingsImpl(*saveSettings);
        if (!saved)
            return Failed(saved.GetError());
    }
    if (saveScene && m_Session.IsDirty())
    {
        auto saved = m_Session.SaveCurrentSceneImpl();
        commands.RecordOperation(CommandActor::Human, "Save Scene before exit", saved);
        if (!saved)
            return Failed(saved.GetError());
    }
    // Keep the guard until teardown: a successful save must not be followed by a late write.
    m_Error.reset();
    m_Accepted = true;
    return Result<void>::Success();
}

Result<void> EditorCloseController::ConfirmSceneChange(PreparedScene& candidate, bool saveScene,
                                                       bool stopRuntime)
{
    if (!IsPending())
        return Failed({ErrorCode::InvalidState, "No scene change confirmation is active."});
    // Preflight before stopping or saving the old document. No public ignoreDirty escape hatch.
    auto valid = m_Session.ValidatePreparedScene(candidate);
    if (!valid)
        return Failed(valid.GetError());
    auto confirmed = Confirm(saveScene, stopRuntime, nullptr);
    if (!confirmed)
        return confirmed;
    // Saving the old document can modify the candidate's file when opening the same path.
    valid = m_Session.ValidatePreparedScene(candidate);
    if (!valid)
    {
        m_Accepted = false;
        return Failed(valid.GetError());
    }
    m_Session.ReplacePreparedScene(candidate);
    m_Accepted = false;
    Cancel();
    return Result<void>::Success();
}
} // namespace Janus::Editor
