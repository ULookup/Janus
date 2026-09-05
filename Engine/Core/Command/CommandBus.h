#pragma once

#include "Core/Command/ICommand.h"
#include "Core/Types.h"

#include <memory>
#include <string_view>
#include <vector>

namespace Janus
{

class CommandBus final
{
public:
    explicit CommandBus(usize maxHistory = 256) noexcept;

    [[nodiscard]] Result<void> Execute(
        std::unique_ptr<ICommand> command);

    [[nodiscard]] Result<void> Undo();
    [[nodiscard]] Result<void> Redo();

    void Clear() noexcept;

    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;

    [[nodiscard]] usize GetHistorySize() const noexcept;
    [[nodiscard]] usize GetCursor() const noexcept;

    [[nodiscard]] std::string_view GetUndoDescription() const noexcept;
    [[nodiscard]] std::string_view GetRedoDescription() const noexcept;

private:
    void TrimHistory() noexcept;

    std::vector<std::unique_ptr<ICommand>> m_History;
    usize m_Cursor = 0;
    usize m_MaxHistory = 256;
};

} // namespace Janus
