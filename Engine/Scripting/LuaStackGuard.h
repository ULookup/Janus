#pragma once

#include "Core/Assert.h"

extern "C"
{
#include <lua.h>
}

namespace Janus::Detail
{

class LuaStackGuard final
{
public:
    explicit LuaStackGuard(lua_State* state) noexcept
        : m_State(state),
          m_InitialTop(lua_gettop(state))
    {
    }

    ~LuaStackGuard()
    {
        JANUS_CORE_ASSERT(
            lua_gettop(m_State) == m_InitialTop,
            "Lua stack imbalance detected.");
    }

    LuaStackGuard(const LuaStackGuard&) = delete;
    LuaStackGuard& operator=(const LuaStackGuard&) = delete;

private:
    lua_State* m_State = nullptr;
    int m_InitialTop = 0;
};

} // namespace Janus::Detail
