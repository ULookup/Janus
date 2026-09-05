#pragma once

#include "Core/Error/Result.h"

#include <string_view>

namespace Janus
{

class ICommand
{
public:
    virtual ~ICommand() = default;

    [[nodiscard]] virtual Result<void> Execute() = 0;
    [[nodiscard]] virtual Result<void> Undo() = 0;
    [[nodiscard]] virtual Result<void> Redo() = 0;

    [[nodiscard]] virtual std::string_view Describe() const noexcept = 0;
};

} // namespace Janus
