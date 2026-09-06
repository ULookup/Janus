#include "Core/Command/CommandBus.h"
#include <algorithm>
#include <chrono>
namespace Janus
{
namespace
{
Result<void> Invalid(std::string text)
{
    return Result<void>::Failure(ErrorCode::InvalidState, std::move(text));
}
} // namespace
std::string_view CommandActorName(CommandActor actor) noexcept
{
    return actor == CommandActor::Agent   ? "Agent"
           : actor == CommandActor::Human ? "Human"
                                          : "System";
}
std::string_view CommandOutcomeName(CommandOutcome outcome) noexcept
{
    switch (outcome)
    {
    case CommandOutcome::Pending:
        return "Pending";
    case CommandOutcome::Committed:
        return "Committed";
    case CommandOutcome::RolledBack:
        return "RolledBack";
    case CommandOutcome::Undone:
        return "Undone";
    case CommandOutcome::Redone:
        return "Redone";
    case CommandOutcome::Failed:
        return "Failed";
    case CommandOutcome::RecoveryRequired:
        return "RecoveryRequired";
    }
    return "Failed";
}
CommandBus::CommandBus(usize maxHistory) noexcept : m_MaxHistory(std::max<usize>(1, maxHistory)) {}
void CommandBus::Publish(CommandReceipt receipt)
{
    // Diagnostics cannot change command outcomes or reenter the bus.
    try
    {
        receipt.sequence = m_NextSequence++;
        receipt.timestampMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count();
        receipt.effectCount = receipt.effects.size();
        receipt.truncated = receipt.description.size() > 1024 || receipt.effects.size() > 16 ||
                            (receipt.error && receipt.error->message.size() > 1024);
        receipt.description.resize(std::min<usize>(receipt.description.size(), 1024));
        if (receipt.effects.size() > 16)
            receipt.effects.resize(16);
        for (auto& effect : receipt.effects)
            if (effect.operation.size() > 64)
            {
                effect.operation.resize(64);
                receipt.truncated = true;
            }
        if (receipt.error && receipt.error->message.size() > 1024)
            receipt.error->message.resize(1024);
        if (m_Activity.size() == 2000)
            m_Activity.pop_front();
        m_Activity.push_back(std::move(receipt));
        if (m_Observer)
        {
            m_Notifying = true;
            m_Observer(m_Activity.back());
            m_Notifying = false;
        }
    }
    catch (...)
    {
        m_Notifying = false;
    }
}
void CommandBus::Emit(const Entry& entry, UUID transaction, CommandActor actor,
                      CommandOutcome outcome, std::optional<Error> error)
{
    try
    {
        Publish({0, entry.id, transaction, actor, outcome, std::string(entry.command->Describe()),
                 entry.command->GetEffects(), std::move(error), 0, false, 0, entry.sessionId});
    }
    catch (...)
    {
    }
}
void CommandBus::RecordOperation(CommandActor actor, std::string description,
                                 const Result<void>& result, UUID sessionId)
{
    if (m_Notifying)
        return;
    Publish({0,
             0,
             GetTransactionId(),
             actor,
             result ? CommandOutcome::Committed : CommandOutcome::Failed,
             std::move(description),
             {},
             result ? std::nullopt : std::optional<Error>(result.GetError()),
             0,
             false,
             0,
             sessionId});
}
Result<UUID> CommandBus::BeginTransaction(CommandActor actor, std::string label)
{
    if (m_Notifying || m_Recovery || m_Pending)
        return Result<UUID>::Failure(ErrorCode::InvalidState, "Authoring transaction unavailable.");
    if (label.size() > 128)
        return Result<UUID>::Failure(ErrorCode::InvalidArgument,
                                     "Transaction label exceeds 128 bytes.");
    m_Pending.emplace();
    m_Pending->label = std::move(label);
    m_Pending->transaction = UUID::Random();
    m_Pending->actor = actor;
    m_PendingBytes = 0;
    return Result<UUID>::Success(m_Pending->transaction);
}
Result<void> CommandBus::Execute(std::unique_ptr<ICommand> command, CommandActor actor,
                                 UUID transaction, UUID sessionId)
{
    if (!command)
        return Result<void>::Failure(ErrorCode::InvalidArgument,
                                     "CommandBus cannot execute a null command.");
    if (m_Notifying || m_Recovery)
        return Invalid("Authoring recovery required or observer reentry denied.");
    if ((m_Pending && (transaction != m_Pending->transaction || actor != m_Pending->actor)) ||
        (!m_Pending && transaction.IsValid()))
        return Invalid("Transaction owner/token mismatch.");
    Entry entry{std::move(command), m_NextId++, actor, sessionId};
    usize bytes = 0;
    if (m_Pending)
    {
        auto estimate = entry.command->EstimateUndoBytes();
        if (!estimate || m_Pending->entries.size() >= 64 ||
            estimate.Value() > 8 * 1024 * 1024 - m_PendingBytes)
        {
            const Error error = estimate
                                    ? Error{ErrorCode::InvalidState,
                                            "Transaction exceeds 64 commands or 8 MiB undo budget."}
                                    : estimate.GetError();
            Emit(entry, transaction, actor, CommandOutcome::Failed, error);
            return Abort(error);
        }
        bytes = estimate.Value();
    }
    auto result = entry.command->Execute();
    if (!result)
    {
        Emit(entry, transaction, actor, CommandOutcome::Failed, result.GetError());
        return m_Pending ? Abort(result.GetError()) : result;
    }
    Emit(entry, transaction, actor,
         m_Pending ? CommandOutcome::Pending : CommandOutcome::Committed);
    if (m_Pending)
    {
        m_Pending->entries.push_back(std::move(entry));
        m_PendingBytes += bytes;
    }
    else
    {
        Group group;
        group.actor = actor;
        group.entries.push_back(std::move(entry));
        AddHistory(std::move(group));
    }
    return Result<void>::Success();
}
void CommandBus::AddHistory(Group group)
{
    if (group.entries.empty())
        return;
    m_History.erase(m_History.begin() + static_cast<std::ptrdiff_t>(m_Cursor), m_History.end());
    m_History.push_back(std::move(group));
    if (m_History.size() > m_MaxHistory)
        m_History.erase(m_History.begin());
    m_Cursor = m_History.size();
}
void CommandBus::Remember(UUID token, bool committed)
{
    if (m_Terminal.size() == 32)
        m_Terminal.pop_front();
    m_Terminal.emplace_back(token, committed);
}
Result<void> CommandBus::CommitTransaction(UUID transaction)
{
    if (m_Notifying || m_Recovery)
        return Invalid("Cannot commit during recovery or observer notification.");
    for (const auto& [token, committed] : m_Terminal)
        if (token == transaction)
            return committed ? Result<void>::Success()
                             : Invalid("Transaction already rolled back.");
    if (!m_Pending || transaction != m_Pending->transaction)
        return Invalid("Unknown transaction token.");
    for (const auto& entry : m_Pending->entries)
        Emit(entry, transaction, entry.actor, CommandOutcome::Committed);
    auto group = std::move(*m_Pending);
    m_Pending.reset();
    m_PendingBytes = 0;
    AddHistory(std::move(group));
    Remember(transaction, true);
    return Result<void>::Success();
}
Result<void> CommandBus::Reverse(Group& group, bool undo, CommandActor actor)
{
    const usize count = group.entries.size();
    for (usize done = 0; done < count; ++done)
    {
        const usize index = undo ? count - 1 - done : done;
        auto result =
            undo ? group.entries[index].command->Undo() : group.entries[index].command->Redo();
        if (!result)
        {
            // Restore already changed members in dependency-safe reverse order.
            for (usize restored = done; restored > 0; --restored)
            {
                const usize prior = undo ? count - restored : restored - 1;
                auto compensated = undo ? group.entries[prior].command->Redo()
                                        : group.entries[prior].command->Undo();
                if (!compensated)
                {
                    m_Recovery = true;
                    Emit(group.entries[prior], group.transaction, CommandActor::System,
                         CommandOutcome::RecoveryRequired, compensated.GetError());
                    return Invalid(
                        "Command group compensation failed; explicit authoring recovery required.");
                }
            }
            Emit(group.entries[index], group.transaction, actor, CommandOutcome::Failed,
                 result.GetError());
            return result;
        }
    }
    return Result<void>::Success();
}
Result<void> CommandBus::RollbackTransaction(UUID transaction, CommandActor actor)
{
    if (m_Notifying || m_Recovery)
        return Invalid("Cannot roll back during recovery or observer notification.");
    for (const auto& [token, committed] : m_Terminal)
        if (token == transaction)
            return !committed ? Result<void>::Success() : Invalid("Transaction already committed.");
    if (!m_Pending || transaction != m_Pending->transaction)
        return Invalid("Unknown transaction token.");
    auto result = Reverse(*m_Pending, true, actor);
    if (!result)
    {
        m_Recovery = true;
        RecordOperation(CommandActor::System, "Transaction rollback requires recovery", result);
        return result;
    }
    for (const auto& entry : m_Pending->entries)
        Emit(entry, transaction, actor, CommandOutcome::RolledBack);
    m_Pending.reset();
    m_PendingBytes = 0;
    Remember(transaction, false);
    return Result<void>::Success();
}
Result<void> CommandBus::Abort(Error error)
{
    auto rollback = RollbackTransaction(GetTransactionId());
    if (!rollback)
        return rollback;
    return Result<void>::Failure(std::move(error));
}
Result<void> CommandBus::Undo()
{
    if (!CanUndo() || m_Notifying)
        return Invalid("CommandBus has no available command to undo.");
    auto& group = m_History[m_Cursor - 1];
    auto result = Reverse(group, true);
    if (!result)
        return result;
    --m_Cursor;
    for (const auto& entry : group.entries)
        Emit(entry, group.transaction, CommandActor::Human, CommandOutcome::Undone);
    return Result<void>::Success();
}
Result<void> CommandBus::Redo()
{
    if (!CanRedo() || m_Notifying)
        return Invalid("CommandBus has no available command to redo.");
    auto& group = m_History[m_Cursor];
    auto result = Reverse(group, false);
    if (!result)
        return result;
    ++m_Cursor;
    for (const auto& entry : group.entries)
        Emit(entry, group.transaction, CommandActor::Human, CommandOutcome::Redone);
    return Result<void>::Success();
}
void CommandBus::Clear() noexcept
{
    if (!m_Pending && !m_Recovery && !m_Notifying)
    {
        m_History.clear();
        m_Cursor = 0;
    }
}
void CommandBus::ResetAfterRecovery() noexcept
{
    if (m_Notifying)
        return;
    m_Pending.reset();
    m_PendingBytes = 0;
    m_Recovery = false;
    m_Terminal.clear();
    Clear();
}
bool CommandBus::CanUndo() const noexcept
{
    return !m_Pending && !m_Recovery && m_Cursor > 0;
}
bool CommandBus::CanRedo() const noexcept
{
    return !m_Pending && !m_Recovery && m_Cursor < m_History.size();
}
usize CommandBus::GetHistorySize() const noexcept
{
    return m_History.size();
}
usize CommandBus::GetCursor() const noexcept
{
    return m_Cursor;
}
std::string_view CommandBus::GetUndoDescription() const noexcept
{
    return CanUndo() ? (m_History[m_Cursor - 1].transaction.IsValid()
                            ? (m_History[m_Cursor - 1].label.empty()
                                   ? std::string_view("Agent transaction")
                                   : std::string_view(m_History[m_Cursor - 1].label))
                            : m_History[m_Cursor - 1].entries.front().command->Describe())
                     : std::string_view{};
}
std::string_view CommandBus::GetRedoDescription() const noexcept
{
    return CanRedo() ? (m_History[m_Cursor].transaction.IsValid()
                            ? (m_History[m_Cursor].label.empty()
                                   ? std::string_view("Agent transaction")
                                   : std::string_view(m_History[m_Cursor].label))
                            : m_History[m_Cursor].entries.front().command->Describe())
                     : std::string_view{};
}
}
