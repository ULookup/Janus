#pragma once

#include "Core/Error/Result.h"

#include "Core/UUID/UUID.h"
#include <string_view>
#include <vector>

namespace Janus
{

struct CommandEffect
{
    UUID entity;
    std::string operation;
    u64 component = 0, property = 0;
};

class ICommand
{
public:
    virtual ~ICommand() = default;

    [[nodiscard]] virtual Result<void> Execute() = 0;
    [[nodiscard]] virtual Result<void> Undo() = 0;
    [[nodiscard]] virtual Result<void> Redo() = 0;

    // Transaction support is opt-in: callers reserve undo memory before mutation.
    virtual Result<usize> EstimateUndoBytes() const
    {
        return Result<usize>::Failure(ErrorCode::InvalidState,
                                      "Command has no bounded transaction contract.");
    }
    virtual std::vector<CommandEffect> GetEffects() const
    {
        return {};
    }

    [[nodiscard]] virtual std::string_view Describe() const noexcept = 0;
};

} // namespace Janus
