#include "Scripting/LuaVirtualMachine.h"

#include "Scripting/LuaStackGuard.h"

extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <memory>
#include <string>
#include <utility>

namespace Janus
{
namespace
{

void OpenLibrary(
    lua_State* state,
    const char* name,
    lua_CFunction openFunction)
{
    luaL_requiref(state, name, openFunction, 1);
    lua_pop(state, 1);
}

void RemoveGlobal(lua_State* state, const char* name)
{
    lua_pushnil(state);
    lua_setglobal(state, name);
}

std::string ReadLuaError(lua_State* state)
{
    const char* message = lua_tostring(state, -1);
    return message != nullptr
        ? std::string(message)
        : std::string("Lua reported a non-string error object.");
}

int TracebackHandler(lua_State* state)
{
    const char* message = lua_tostring(state, 1);
    if (message != nullptr)
    {
        luaL_traceback(state, state, message, 1);
    }
    else
    {
        lua_pushliteral(state, "Lua error (non-string error object)");
    }

    return 1;
}

std::string BuildErrorMessage(
    std::string_view phase,
    std::string_view chunkName,
    std::string_view luaMessage)
{
    std::string message;
    message.reserve(
        phase.size()
        + chunkName.size()
        + luaMessage.size()
        + 24);

    message.append("Lua ");
    message.append(phase);
    message.append(" failed for ");
    message.append(chunkName);
    message.append(": ");
    message.append(luaMessage);
    return message;
}

} // namespace

struct LuaVirtualMachine::Impl
{
    ~Impl()
    {
        if (state != nullptr)
        {
            lua_close(state);
            state = nullptr;
        }
    }

    lua_State* state = nullptr;
};

Result<std::unique_ptr<LuaVirtualMachine>> LuaVirtualMachine::Create()
{
    auto impl = std::make_unique<Impl>();
    impl->state = luaL_newstate();

    if (impl->state == nullptr)
    {
        return Result<std::unique_ptr<LuaVirtualMachine>>::Failure(
            ErrorCode::ScriptVmCreateFailed,
            "Failed to create Lua VM state.");
    }

    lua_State* state = impl->state;

    OpenLibrary(state, LUA_GNAME, luaopen_base);
    OpenLibrary(state, LUA_MATHLIBNAME, luaopen_math);
    OpenLibrary(state, LUA_STRLIBNAME, luaopen_string);
    OpenLibrary(state, LUA_TABLIBNAME, luaopen_table);
    OpenLibrary(state, LUA_UTF8LIBNAME, luaopen_utf8);

    // Base Lua exposes file-loading helpers even without the io/package
    // libraries. Gameplay scripts must obtain project data through Janus
    // capabilities instead of opening arbitrary filesystem paths directly.
    RemoveGlobal(state, "dofile");
    RemoveGlobal(state, "loadfile");

    return Result<std::unique_ptr<LuaVirtualMachine>>::Success(
        std::unique_ptr<LuaVirtualMachine>(
            new LuaVirtualMachine(std::move(impl))));
}

LuaVirtualMachine::LuaVirtualMachine(
    std::unique_ptr<Impl> impl) noexcept
    : m_Impl(std::move(impl))
{
}

LuaVirtualMachine::~LuaVirtualMachine() = default;

Result<void> LuaVirtualMachine::Execute(
    std::string_view source,
    std::string_view chunkName)
{
    lua_State* state = m_Impl->state;
    Detail::LuaStackGuard stackGuard(state);
    const int initialTop = lua_gettop(state);

    lua_pushcfunction(state, TracebackHandler);
    const int errorHandlerIndex = lua_gettop(state);

    const std::string stableChunkName = chunkName.empty()
        ? std::string("<memory>")
        : std::string(chunkName);

    const int loadStatus = luaL_loadbufferx(
        state,
        source.data(),
        source.size(),
        stableChunkName.c_str(),
        nullptr);

    if (loadStatus != LUA_OK)
    {
        const std::string luaMessage = ReadLuaError(state);
        lua_settop(state, initialTop);
        return Result<void>::Failure(
            ErrorCode::ScriptCompileFailed,
            BuildErrorMessage(
                "compile",
                stableChunkName,
                luaMessage));
    }

    const int callStatus = lua_pcall(
        state,
        0,
        0,
        errorHandlerIndex);

    if (callStatus != LUA_OK)
    {
        const std::string luaMessage = ReadLuaError(state);
        lua_settop(state, initialTop);
        return Result<void>::Failure(
            ErrorCode::ScriptRuntimeFailed,
            BuildErrorMessage(
                "runtime",
                stableChunkName,
                luaMessage));
    }

    lua_settop(state, initialTop);
    return Result<void>::Success();
}

int LuaVirtualMachine::GetStackTop() const noexcept
{
    return lua_gettop(m_Impl->state);
}

void* LuaVirtualMachine::GetNativeState() noexcept
{
    return m_Impl->state;
}

} // namespace Janus
