#include "Core/Command/CommandBus.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace Janus
{

CommandBus::CommandBus(usize maxHistory) noexcept
    : m_MaxHistory(std::max<usize>(1, maxHistory))
{
}

Result<void> CommandBus::Execute(
    std::unique_ptr<ICommand> command)
{
    if (command == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "CommandBus cannot execute a null command.");
    }

    auto executed = command->Execute();
    if (!executed)
    {
        return executed;
    }

    if (m_Cursor < m_History.size())
    {
        m_History.erase(
            m_History.begin()
                + static_cast<std::ptrdiff_t>(m_Cursor),
            m_History.end());
    }

    m_History.push_back(std::move(command));
    m_Cursor = m_History.size();
    TrimHistory();

    return Result<void>::Success();
}

Result<void> CommandBus::Undo()
{
    if (!CanUndo())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "CommandBus has no command to undo.");
    }

    ICommand& command =
        *m_History[m_Cursor - 1];

    auto undone = command.Undo();
    if (!undone)
    {
        return undone;
    }

    --m_Cursor;
    return Result<void>::Success();
}

Result<void> CommandBus::Redo()
{
    if (!CanRedo())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "CommandBus has no command to redo.");
    }

    ICommand& command =
        *m_History[m_Cursor];

    auto redone = command.Redo();
    if (!redone)
    {
        return redone;
    }

    ++m_Cursor;
    return Result<void>::Success();
}

void CommandBus::Clear() noexcept
{
    m_History.clear();
    m_Cursor = 0;
}

bool CommandBus::CanUndo() const noexcept
{
    return m_Cursor > 0;
}

bool CommandBus::CanRedo() const noexcept
{
    return m_Cursor < m_History.size();
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
    if (!CanUndo())
    {
        return {};
    }

    return m_History[m_Cursor - 1]->Describe();
}

std::string_view CommandBus::GetRedoDescription() const noexcept
{
    if (!CanRedo())
    {
        return {};
    }

    return m_History[m_Cursor]->Describe();
}

void CommandBus::TrimHistory() noexcept
{
    if (m_History.size() <= m_MaxHistory)
    {
        return;
    }

    const usize removeCount =
        m_History.size() - m_MaxHistory;

    m_History.erase(
        m_History.begin(),
        m_History.begin()
            + static_cast<std::ptrdiff_t>(removeCount));

    m_Cursor = m_History.size();
}

} // namespace Janus
