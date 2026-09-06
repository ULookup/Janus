#pragma once
#include "Core/Command/ICommand.h"
#include <deque>
#include <functional>
#include <memory>
#include <optional>
namespace Janus
{
enum class CommandActor
{
    Human,
    Agent,
    System
};
enum class CommandOutcome
{
    Pending,
    Committed,
    RolledBack,
    Undone,
    Redone,
    Failed,
    RecoveryRequired
};
std::string_view CommandActorName(CommandActor actor) noexcept;
std::string_view CommandOutcomeName(CommandOutcome outcome) noexcept;
struct CommandReceipt
{
    u64 sequence = 0, commandId = 0;
    UUID transaction;
    CommandActor actor = CommandActor::Human;
    CommandOutcome outcome = CommandOutcome::Committed;
    std::string description;
    std::vector<CommandEffect> effects;
    std::optional<Error> error;
    i64 timestampMilliseconds = 0;
    bool truncated = false;
    usize effectCount = 0;
    UUID sessionId;
};
class CommandBus final
{
public:
    explicit CommandBus(usize maxHistory = 256) noexcept;
    Result<void> Execute(std::unique_ptr<ICommand> command,
                         CommandActor actor = CommandActor::Human, UUID transaction = {},
                         UUID sessionId = {});
    Result<void> Undo();
    Result<void> Redo();
    Result<UUID> BeginTransaction(CommandActor actor, std::string label = {});
    Result<void> CommitTransaction(UUID transaction);
    Result<void> RollbackTransaction(UUID transaction, CommandActor actor = CommandActor::System);
    bool HasTransaction() const noexcept
    {
        return m_Pending.has_value();
    }
    bool RecoveryRequired() const noexcept
    {
        return m_Recovery;
    }
    UUID GetTransactionId() const noexcept
    {
        return m_Pending ? m_Pending->transaction : UUID{};
    }
    usize GetPendingCount() const noexcept
    {
        return m_Pending ? m_Pending->entries.size() : 0;
    }
    usize GetPendingBytes() const noexcept
    {
        return m_PendingBytes;
    }
    const std::deque<CommandReceipt>& GetActivity() const noexcept
    {
        return m_Activity;
    }
    u64 GetNextActivitySequence() const noexcept
    {
        return m_NextSequence;
    }
    void RecordOperation(CommandActor actor, std::string description, const Result<void>& result,
                         UUID sessionId = {});
    void SetObserver(std::function<void(const CommandReceipt&)> observer)
    {
        m_Observer = std::move(observer);
    }
    void Clear() noexcept;
    // Only the host's explicit discard/reload recovery action may reset a poisoned bus.
    void ResetAfterRecovery() noexcept;
    bool CanUndo() const noexcept;
    bool CanRedo() const noexcept;
    usize GetHistorySize() const noexcept;
    usize GetCursor() const noexcept;
    std::string_view GetUndoDescription() const noexcept;
    std::string_view GetRedoDescription() const noexcept;

  private:
    struct Entry
    {
        std::unique_ptr<ICommand> command;
        u64 id;
        CommandActor actor;
        UUID sessionId;
    };
    struct Group
    {
        std::vector<Entry> entries;
        UUID transaction;
        CommandActor actor = CommandActor::Human;
        std::string label;
    };
    void AddHistory(Group group);
    void Emit(const Entry& entry, UUID transaction, CommandActor actor, CommandOutcome outcome,
              std::optional<Error> error = {});
    void Publish(CommandReceipt receipt);
    Result<void> Reverse(Group& group, bool undo, CommandActor actor = CommandActor::Human);
    Result<void> Abort(Error error);
    void Remember(UUID token, bool committed);
    std::vector<Group> m_History;
    usize m_Cursor = 0, m_MaxHistory = 256, m_PendingBytes = 0;
    std::optional<Group> m_Pending;
    bool m_Recovery = false, m_Notifying = false;
    u64 m_NextId = 1, m_NextSequence = 1;
    std::deque<std::pair<UUID, bool>> m_Terminal;
    std::deque<CommandReceipt> m_Activity;
    std::function<void(const CommandReceipt&)> m_Observer;
};
} // namespace Janus
