#pragma once

#include "Core/Error/Result.h"

#include <memory>
#include <string_view>

namespace Janus
{
namespace Detail
{
struct LuaVirtualMachineAccess;
struct LuaVirtualMachineTestAccess;
}

class LuaVirtualMachine final
{
public:
    [[nodiscard]] static Result<std::unique_ptr<LuaVirtualMachine>> Create();

    ~LuaVirtualMachine();

    LuaVirtualMachine(const LuaVirtualMachine&) = delete;
    LuaVirtualMachine& operator=(const LuaVirtualMachine&) = delete;
    LuaVirtualMachine(LuaVirtualMachine&&) = delete;
    LuaVirtualMachine& operator=(LuaVirtualMachine&&) = delete;

    [[nodiscard]] Result<void> Execute(
        std::string_view source,
        std::string_view chunkName = "<memory>");

private:
    struct Impl;

    explicit LuaVirtualMachine(std::unique_ptr<Impl> impl) noexcept;

    [[nodiscard]] int GetStackTop() const noexcept;
    [[nodiscard]] void* GetNativeState() noexcept;

    std::unique_ptr<Impl> m_Impl;

    friend struct Detail::LuaVirtualMachineAccess;
    friend struct Detail::LuaVirtualMachineTestAccess;
};

} // namespace Janus
