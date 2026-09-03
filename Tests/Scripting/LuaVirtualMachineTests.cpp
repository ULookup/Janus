#include "Scripting/LuaVirtualMachine.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <utility>

namespace Janus::Detail
{

struct LuaVirtualMachineTestAccess
{
    static int StackTop(const LuaVirtualMachine& vm) noexcept
    {
        return vm.GetStackTop();
    }
};

} // namespace Janus::Detail

namespace
{

std::unique_ptr<Janus::LuaVirtualMachine> CreateVm()
{
    auto result = Janus::LuaVirtualMachine::Create();
    REQUIRE(result);
    return std::move(result).Value();
}

} // namespace

TEST_CASE("Lua VM exposes only the approved gameplay libraries",
          "[scripting][lua][v0.5]")
{
    auto vm = CreateVm();

    const auto result = vm->Execute(R"lua(
        assert(type(_G) == "table")
        assert(type(math) == "table")
        assert(type(string) == "table")
        assert(type(table) == "table")
        assert(type(utf8) == "table")

        assert(io == nil)
        assert(os == nil)
        assert(package == nil)
        assert(debug == nil)
        assert(coroutine == nil)

        assert(dofile == nil)
        assert(loadfile == nil)
    )lua", "LibraryPolicy.lua");

    REQUIRE(result);
}

TEST_CASE("Lua VM reports compile errors without leaking stack entries",
          "[scripting][lua][v0.5]")
{
    auto vm = CreateVm();
    const int initialTop =
        Janus::Detail::LuaVirtualMachineTestAccess::StackTop(*vm);

    const auto result = vm->Execute(
        "local broken = ",
        "Broken.lua");

    REQUIRE_FALSE(result);
    REQUIRE(result.GetError().code == Janus::ErrorCode::ScriptCompileFailed);
    REQUIRE(result.GetError().message.find("Broken.lua") != std::string::npos);
    REQUIRE(
        Janus::Detail::LuaVirtualMachineTestAccess::StackTop(*vm)
        == initialTop);
}

TEST_CASE("Lua VM reports runtime errors with traceback context",
          "[scripting][lua][v0.5]")
{
    auto vm = CreateVm();
    const int initialTop =
        Janus::Detail::LuaVirtualMachineTestAccess::StackTop(*vm);

    const auto result = vm->Execute(R"lua(
        local function inner()
            error("boom")
        end

        inner()
    )lua", "RuntimeFailure.lua");

    REQUIRE_FALSE(result);
    REQUIRE(result.GetError().code == Janus::ErrorCode::ScriptRuntimeFailed);
    REQUIRE(result.GetError().message.find("RuntimeFailure.lua")
        != std::string::npos);
    REQUIRE(result.GetError().message.find("boom") != std::string::npos);
    REQUIRE(result.GetError().message.find("stack traceback")
        != std::string::npos);
    REQUIRE(
        Janus::Detail::LuaVirtualMachineTestAccess::StackTop(*vm)
        == initialTop);
}

TEST_CASE("Lua VM remains reusable after protected execution failures",
          "[scripting][lua][v0.5]")
{
    auto vm = CreateVm();
    const int initialTop =
        Janus::Detail::LuaVirtualMachineTestAccess::StackTop(*vm);

    for (int iteration = 0; iteration < 32; ++iteration)
    {
        const auto failed = vm->Execute(
            "error('expected failure')",
            "ExpectedFailure.lua");
        REQUIRE_FALSE(failed);
        REQUIRE(
            Janus::Detail::LuaVirtualMachineTestAccess::StackTop(*vm)
            == initialTop);
    }

    REQUIRE(vm->Execute(
        "local value = math.floor(3.75); assert(value == 3)",
        "Recovery.lua"));
    REQUIRE(
        Janus::Detail::LuaVirtualMachineTestAccess::StackTop(*vm)
        == initialTop);
}

TEST_CASE("Lua VM supports repeated create and destroy cycles",
          "[scripting][lua][v0.5]")
{
    for (int iteration = 0; iteration < 16; ++iteration)
    {
        auto vm = CreateVm();
        REQUIRE(vm->Execute(
            "assert(string.upper('janus') == 'JANUS')",
            "Lifecycle.lua"));
    }
}
