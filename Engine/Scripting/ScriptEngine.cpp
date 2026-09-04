#include "Scripting/ScriptEngine.h"

#include "Asset/AssetService.h"
#include "Core/Input/InputState.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scripting/LuaStackGuard.h"
#include "Scripting/LuaVirtualMachine.h"

extern "C"
{
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <array>
#include <filesystem>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Janus
{
namespace Detail
{

struct LuaVirtualMachineAccess
{
    [[nodiscard]] static lua_State* State(LuaVirtualMachine& machine) noexcept
    {
        return static_cast<lua_State*>(machine.GetNativeState());
    }
};

} // namespace Detail

namespace
{

constexpr const char* EntityMetatableName = "Janus.Entity";
char BindingContextRegistryKey = 0;

struct BindingContext
{
    Scene* scene = nullptr;
    const InputState* input = nullptr;
};

struct LuaEntityRef
{
    UUID::Storage bytes{};
};

struct ScriptInstance
{
    UUID entityId;
    AssetHandle script;
    int tableRegistryRef = LUA_NOREF;
};

struct DesiredInstance
{
    UUID entityId;
    AssetHandle script;
};

struct KeyBinding
{
    std::string_view name;
    KeyCode key;
};

constexpr std::array<KeyBinding, 21> KeyBindings{{
    {"Escape", KeyCode::Escape},
    {"Space", KeyCode::Space},
    {"Enter", KeyCode::Enter},
    {"ArrowUp", KeyCode::ArrowUp},
    {"ArrowDown", KeyCode::ArrowDown},
    {"ArrowLeft", KeyCode::ArrowLeft},
    {"ArrowRight", KeyCode::ArrowRight},
    {"W", KeyCode::W},
    {"A", KeyCode::A},
    {"S", KeyCode::S},
    {"D", KeyCode::D},
    {"Digit0", KeyCode::Digit0},
    {"Digit1", KeyCode::Digit1},
    {"Digit2", KeyCode::Digit2},
    {"Digit3", KeyCode::Digit3},
    {"Digit4", KeyCode::Digit4},
    {"Digit5", KeyCode::Digit5},
    {"Digit6", KeyCode::Digit6},
    {"Digit7", KeyCode::Digit7},
    {"Digit8", KeyCode::Digit8},
    {"Digit9", KeyCode::Digit9}}};

[[nodiscard]] std::optional<KeyCode> ParseKeyCode(std::string_view name) noexcept
{
    for (const KeyBinding& binding : KeyBindings)
    {
        if (binding.name == name)
        {
            return binding.key;
        }
    }
    return std::nullopt;
}

[[nodiscard]] BindingContext* GetBindingContext(lua_State* state)
{
    lua_pushlightuserdata(state, &BindingContextRegistryKey);
    lua_rawget(state, LUA_REGISTRYINDEX);
    auto* context = static_cast<BindingContext*>(lua_touserdata(state, -1));
    lua_pop(state, 1);

    if (context == nullptr || context->scene == nullptr || context->input == nullptr)
    {
        luaL_error(state, "Janus scripting binding context is unavailable.");
        return nullptr;
    }
    return context;
}

[[nodiscard]] LuaEntityRef* CheckEntityRef(lua_State* state)
{
    return static_cast<LuaEntityRef*>(
        luaL_checkudata(state, 1, EntityMetatableName));
}

[[nodiscard]] ECS::Entity ResolveEntity(
    lua_State* state,
    BindingContext& context,
    const LuaEntityRef& reference)
{
    const UUID entityId(reference.bytes);
    const ECS::Entity entity = context.scene->FindEntity(entityId);
    if (!entity.IsValid())
    {
        luaL_error(state, "Janus Entity reference is stale.");
        return {};
    }
    return entity;
}

int EntityId(lua_State* state)
{
    BindingContext* context = GetBindingContext(state);
    LuaEntityRef* reference = CheckEntityRef(state);
    const ECS::Entity entity = ResolveEntity(state, *context, *reference);
    const auto* identity =
        context->scene->GetComponent<EntityIdentityComponent>(entity);
    if (identity == nullptr)
    {
        return luaL_error(state, "Janus Entity is missing EntityIdentityComponent.");
    }

    const std::string value = identity->id.ToString();
    lua_pushlstring(state, value.data(), value.size());
    return 1;
}

int EntityName(lua_State* state)
{
    BindingContext* context = GetBindingContext(state);
    LuaEntityRef* reference = CheckEntityRef(state);
    const ECS::Entity entity = ResolveEntity(state, *context, *reference);
    const auto* identity =
        context->scene->GetComponent<EntityIdentityComponent>(entity);
    if (identity == nullptr)
    {
        return luaL_error(state, "Janus Entity is missing EntityIdentityComponent.");
    }

    lua_pushlstring(state, identity->name.data(), identity->name.size());
    return 1;
}

int EntityGetPosition(lua_State* state)
{
    BindingContext* context = GetBindingContext(state);
    LuaEntityRef* reference = CheckEntityRef(state);
    const ECS::Entity entity = ResolveEntity(state, *context, *reference);
    const auto* transform =
        context->scene->GetComponent<TransformComponent>(entity);
    if (transform == nullptr)
    {
        return luaL_error(state, "Janus Entity is missing TransformComponent.");
    }

    lua_pushnumber(state, static_cast<lua_Number>(transform->position.x));
    lua_pushnumber(state, static_cast<lua_Number>(transform->position.y));
    return 2;
}

int EntitySetPosition(lua_State* state)
{
    BindingContext* context = GetBindingContext(state);
    LuaEntityRef* reference = CheckEntityRef(state);
    const ECS::Entity entity = ResolveEntity(state, *context, *reference);
    auto* transform = context->scene->GetComponent<TransformComponent>(entity);
    if (transform == nullptr)
    {
        return luaL_error(state, "Janus Entity is missing TransformComponent.");
    }

    transform->position.x = static_cast<f32>(luaL_checknumber(state, 2));
    transform->position.y = static_cast<f32>(luaL_checknumber(state, 3));
    transform->dirty = true;
    return 0;
}

[[nodiscard]] KeyCode CheckKeyCode(lua_State* state)
{
    const char* name = luaL_checkstring(state, 1);
    const auto key = ParseKeyCode(name);
    if (!key.has_value())
    {
        luaL_error(state, "Unknown Janus key name '%s'.", name);
        return KeyCode::Escape;
    }
    return *key;
}

int InputIsKeyDown(lua_State* state)
{
    BindingContext* context = GetBindingContext(state);
    lua_pushboolean(state, context->input->IsKeyDown(CheckKeyCode(state)));
    return 1;
}

int InputWasKeyPressed(lua_State* state)
{
    BindingContext* context = GetBindingContext(state);
    lua_pushboolean(state, context->input->WasKeyPressed(CheckKeyCode(state)));
    return 1;
}

int InputWasKeyReleased(lua_State* state)
{
    BindingContext* context = GetBindingContext(state);
    lua_pushboolean(state, context->input->WasKeyReleased(CheckKeyCode(state)));
    return 1;
}

int ReadOnlyNewIndex(lua_State* state)
{
    return luaL_error(state, "Input table is read-only.");
}

void PushEntityReference(lua_State* state, UUID entityId)
{
    void* storage = lua_newuserdatauv(state, sizeof(LuaEntityRef), 0);
    new (storage) LuaEntityRef{entityId.GetBytes()};
    luaL_getmetatable(state, EntityMetatableName);
    lua_setmetatable(state, -2);
}

void RegisterEntityBinding(lua_State* state)
{
    if (luaL_newmetatable(state, EntityMetatableName) != 0)
    {
        static const luaL_Reg Methods[] = {
            {"id", EntityId},
            {"name", EntityName},
            {"get_position", EntityGetPosition},
            {"set_position", EntitySetPosition},
            {nullptr, nullptr}};

        lua_pushvalue(state, -1);
        lua_setfield(state, -2, "__index");
        luaL_setfuncs(state, Methods, 0);
        lua_pushliteral(state, "locked");
        lua_setfield(state, -2, "__metatable");
    }
    lua_pop(state, 1);
}

void RegisterInputBinding(lua_State* state)
{
    static const luaL_Reg Methods[] = {
        {"is_key_down", InputIsKeyDown},
        {"was_key_pressed", InputWasKeyPressed},
        {"was_key_released", InputWasKeyReleased},
        {nullptr, nullptr}};

    lua_newtable(state);
    lua_newtable(state);
    lua_newtable(state);
    luaL_setfuncs(state, Methods, 0);
    lua_setfield(state, -2, "__index");
    lua_pushcfunction(state, ReadOnlyNewIndex);
    lua_setfield(state, -2, "__newindex");
    lua_pushliteral(state, "locked");
    lua_setfield(state, -2, "__metatable");
    lua_setmetatable(state, -2);
    lua_setglobal(state, "Input");
}

[[nodiscard]] std::string ReadLuaError(lua_State* state)
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

[[nodiscard]] std::string ScriptContext(
    std::string_view phase,
    const ScriptInstance& instance,
    std::string_view detail)
{
    std::string message;
    message.reserve(phase.size() + detail.size() + 128);
    message.append("Lua ");
    message.append(phase);
    message.append(" failed for entity ");
    message.append(instance.entityId.ToString());
    message.append(" script ");
    message.append(instance.script.ToString());
    message.append(": ");
    message.append(detail);
    return message;
}

} // namespace

struct ScriptEngine::Impl
{
    Impl(
        Scene& sceneRef,
        AssetService& assetService,
        const InputState& inputState,
        std::unique_ptr<LuaVirtualMachine> machine)
        : scene(sceneRef),
          assets(assetService),
          input(inputState),
          virtualMachine(std::move(machine)),
          bindingContext{&scene, &input}
    {
    }

    [[nodiscard]] lua_State* State() noexcept
    {
        return Detail::LuaVirtualMachineAccess::State(*virtualMachine);
    }

    void InitializeBindings()
    {
        lua_State* state = State();
        Detail::LuaStackGuard stackGuard(state);
        const int initialTop = lua_gettop(state);

        lua_pushlightuserdata(state, &BindingContextRegistryKey);
        lua_pushlightuserdata(state, &bindingContext);
        lua_rawset(state, LUA_REGISTRYINDEX);
        RegisterEntityBinding(state);
        RegisterInputBinding(state);

        lua_settop(state, initialTop);
    }

    [[nodiscard]] Result<std::vector<DesiredInstance>> BuildDesiredInstances()
    {
        std::vector<DesiredInstance> desired;
        for (const ECS::Entity entity : scene.GetEntities())
        {
            const auto* script = scene.GetComponent<LuaScriptComponent>(entity);
            if (script == nullptr || !script->enabled)
            {
                continue;
            }

            const auto* identity =
                scene.GetComponent<EntityIdentityComponent>(entity);
            if (identity == nullptr)
            {
                return Result<std::vector<DesiredInstance>>::Failure(
                    ErrorCode::InvalidState,
                    "Scripted Scene entity is missing persistent identity.");
            }
            if (!script->script.IsValid())
            {
                return Result<std::vector<DesiredInstance>>::Failure(
                    ErrorCode::InvalidState,
                    "Enabled LuaScript component on entity "
                        + identity->id.ToString()
                        + " has an invalid Script AssetHandle.");
            }

            desired.push_back(DesiredInstance{identity->id, script->script});
        }

        std::sort(
            desired.begin(),
            desired.end(),
            [](const DesiredInstance& left, const DesiredInstance& right)
            {
                return left.entityId < right.entityId;
            });
        return Result<std::vector<DesiredInstance>>::Success(std::move(desired));
    }

    [[nodiscard]] Result<void> CallCallback(
        const ScriptInstance& instance,
        std::string_view callback,
        std::optional<f64> deltaSeconds = std::nullopt)
    {
        lua_State* state = State();
        Detail::LuaStackGuard stackGuard(state);
        const int initialTop = lua_gettop(state);

        lua_pushcfunction(state, TracebackHandler);
        const int errorHandlerIndex = lua_gettop(state);
        lua_rawgeti(state, LUA_REGISTRYINDEX, instance.tableRegistryRef);
        if (!lua_istable(state, -1))
        {
            lua_settop(state, initialTop);
            return Result<void>::Failure(
                ErrorCode::ScriptRuntimeFailed,
                ScriptContext(
                    callback,
                    instance,
                    "instance registry reference is not a table."));
        }

        const int tableIndex = lua_gettop(state);
        lua_pushlstring(state, callback.data(), callback.size());
        lua_rawget(state, tableIndex);
        if (lua_isnil(state, -1))
        {
            lua_settop(state, initialTop);
            return Result<void>::Success();
        }
        if (!lua_isfunction(state, -1))
        {
            lua_settop(state, initialTop);
            return Result<void>::Failure(
                ErrorCode::ScriptRuntimeFailed,
                ScriptContext(
                    callback,
                    instance,
                    "callback exists but is not a function."));
        }

        lua_pushvalue(state, tableIndex);
        int argumentCount = 1;
        if (deltaSeconds.has_value())
        {
            lua_pushnumber(state, static_cast<lua_Number>(*deltaSeconds));
            ++argumentCount;
        }

        const int callStatus = lua_pcall(
            state,
            argumentCount,
            0,
            errorHandlerIndex);
        if (callStatus != LUA_OK)
        {
            const std::string luaMessage = ReadLuaError(state);
            lua_settop(state, initialTop);
            return Result<void>::Failure(
                ErrorCode::ScriptRuntimeFailed,
                ScriptContext(callback, instance, luaMessage));
        }

        lua_settop(state, initialTop);
        return Result<void>::Success();
    }

    [[nodiscard]] Result<ScriptInstance> CreateInstance(
        UUID entityId,
        AssetHandle scriptHandle)
    {
        auto source = assets.LoadLuaScriptSource(scriptHandle);
        if (!source)
        {
            const Error& assetError = source.GetError();
            return Result<ScriptInstance>::Failure(
                Error{
                    assetError.code,
                    "Failed to load Lua script " + scriptHandle.ToString()
                        + " for entity " + entityId.ToString()
                        + ": " + assetError.message});
        }

        lua_State* state = State();
        Detail::LuaStackGuard stackGuard(state);
        const int initialTop = lua_gettop(state);
        lua_pushcfunction(state, TracebackHandler);
        const int errorHandlerIndex = lua_gettop(state);

        const std::string chunkName = "@script:" + scriptHandle.ToString();
        const std::string_view sourceText = source.Value();
        const int loadStatus = luaL_loadbufferx(
            state,
            sourceText.data(),
            sourceText.size(),
            chunkName.c_str(),
            nullptr);
        if (loadStatus != LUA_OK)
        {
            const std::string luaMessage = ReadLuaError(state);
            lua_settop(state, initialTop);
            return Result<ScriptInstance>::Failure(
                ErrorCode::ScriptCompileFailed,
                "Lua compile failed for entity " + entityId.ToString()
                    + " script " + scriptHandle.ToString()
                    + ": " + luaMessage);
        }

        const int callStatus = lua_pcall(
            state,
            0,
            LUA_MULTRET,
            errorHandlerIndex);
        if (callStatus != LUA_OK)
        {
            const std::string luaMessage = ReadLuaError(state);
            lua_settop(state, initialTop);
            return Result<ScriptInstance>::Failure(
                ErrorCode::ScriptRuntimeFailed,
                "Lua load failed for entity " + entityId.ToString()
                    + " script " + scriptHandle.ToString()
                    + ": " + luaMessage);
        }

        const int returnCount = lua_gettop(state) - errorHandlerIndex;
        if (returnCount != 1 || !lua_istable(state, -1))
        {
            lua_settop(state, initialTop);
            return Result<ScriptInstance>::Failure(
                ErrorCode::ScriptRuntimeFailed,
                "Lua script " + scriptHandle.ToString()
                    + " for entity " + entityId.ToString()
                    + " must return exactly one table.");
        }

        const int tableIndex = lua_gettop(state);
        lua_pushliteral(state, "entity");
        PushEntityReference(state, entityId);
        lua_rawset(state, tableIndex);

        ScriptInstance instance{
            entityId,
            scriptHandle,
            luaL_ref(state, LUA_REGISTRYINDEX)};
        lua_settop(state, initialTop);

        auto created = CallCallback(instance, "OnCreate");
        if (!created)
        {
            luaL_unref(state, LUA_REGISTRYINDEX, instance.tableRegistryRef);
            instance.tableRegistryRef = LUA_NOREF;
            return Result<ScriptInstance>::Failure(created.GetError());
        }
        return Result<ScriptInstance>::Success(instance);
    }

    [[nodiscard]] Result<void> DestroyInstance(ScriptInstance& instance)
    {
        auto destroyed = CallCallback(instance, "OnDestroy");
        luaL_unref(State(), LUA_REGISTRYINDEX, instance.tableRegistryRef);
        instance.tableRegistryRef = LUA_NOREF;
        return destroyed;
    }

    [[nodiscard]] std::vector<UUID> SortedInstanceIds() const
    {
        std::vector<UUID> ids;
        ids.reserve(instances.size());
        for (const auto& [entityId, instance] : instances)
        {
            static_cast<void>(instance);
            ids.push_back(entityId);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    [[nodiscard]] Result<void> Reconcile()
    {
        auto desiredResult = BuildDesiredInstances();
        if (!desiredResult)
        {
            return Result<void>::Failure(desiredResult.GetError());
        }
        std::vector<DesiredInstance> desired =
            std::move(desiredResult).Value();

        std::unordered_map<UUID, AssetHandle, UUIDHash> desiredById;
        desiredById.reserve(desired.size());
        for (const DesiredInstance& entry : desired)
        {
            desiredById.emplace(entry.entityId, entry.script);
        }

        std::vector<UUID> toDestroy;
        for (const auto& [entityId, instance] : instances)
        {
            const auto desiredIterator = desiredById.find(entityId);
            if (desiredIterator == desiredById.end()
                || desiredIterator->second != instance.script)
            {
                toDestroy.push_back(entityId);
            }
        }
        std::sort(toDestroy.begin(), toDestroy.end());

        std::optional<Error> firstError;
        for (const UUID& entityId : toDestroy)
        {
            auto iterator = instances.find(entityId);
            if (iterator == instances.end())
            {
                continue;
            }
            auto destroyed = DestroyInstance(iterator->second);
            if (!destroyed && !firstError.has_value())
            {
                firstError = destroyed.GetError();
            }
            instances.erase(iterator);
        }

        for (const DesiredInstance& entry : desired)
        {
            if (instances.contains(entry.entityId))
            {
                continue;
            }
            auto created = CreateInstance(entry.entityId, entry.script);
            if (!created)
            {
                if (!firstError.has_value())
                {
                    firstError = created.GetError();
                }
                continue;
            }
            instances.emplace(entry.entityId, std::move(created).Value());
        }

        if (firstError.has_value())
        {
            return Result<void>::Failure(std::move(*firstError));
        }
        return Result<void>::Success();
    }

    [[nodiscard]] std::vector<AssetHandle> DesiredScriptHandles(
        const std::vector<DesiredInstance>& desired) const
    {
        std::vector<AssetHandle> handles;
        handles.reserve(desired.size());
        for (const DesiredInstance& entry : desired)
        {
            handles.push_back(entry.script);
        }

        std::sort(handles.begin(), handles.end());
        handles.erase(
            std::unique(handles.begin(), handles.end()),
            handles.end());
        return handles;
    }

    [[nodiscard]] Result<void> CaptureActiveWriteTimes()
    {
        scriptWriteTimes.clear();

        std::vector<AssetHandle> handles;
        handles.reserve(instances.size());
        for (const auto& [entityId, instance] : instances)
        {
            static_cast<void>(entityId);
            handles.push_back(instance.script);
        }

        std::sort(handles.begin(), handles.end());
        handles.erase(
            std::unique(handles.begin(), handles.end()),
            handles.end());

        for (const AssetHandle handle : handles)
        {
            auto time = assets.GetLastWriteTime(handle);
            if (!time)
            {
                return Result<void>::Failure(time.GetError());
            }
            scriptWriteTimes.emplace(handle, time.Value());
        }

        return Result<void>::Success();
    }

    [[nodiscard]] Result<void> ReloadScript(
        AssetHandle handle,
        const std::vector<DesiredInstance>& desired,
        std::filesystem::file_time_type currentWriteTime)
    {
        std::vector<UUID> existingIds;
        for (const auto& [entityId, instance] : instances)
        {
            if (instance.script == handle)
            {
                existingIds.push_back(entityId);
            }
        }
        std::sort(existingIds.begin(), existingIds.end());

        std::optional<Error> destroyError;
        for (const UUID& entityId : existingIds)
        {
            auto iterator = instances.find(entityId);
            if (iterator == instances.end())
            {
                continue;
            }

            auto destroyed = DestroyInstance(iterator->second);
            if (!destroyed && !destroyError.has_value())
            {
                destroyError = destroyed.GetError();
            }
            instances.erase(iterator);
        }

        static_cast<void>(assets.Unload(handle));

        if (destroyError.has_value())
        {
            return Result<void>::Failure(std::move(*destroyError));
        }

        std::vector<UUID> createdIds;
        for (const DesiredInstance& entry : desired)
        {
            if (entry.script != handle)
            {
                continue;
            }

            auto created = CreateInstance(entry.entityId, entry.script);
            if (!created)
            {
                for (const UUID& createdId : createdIds)
                {
                    auto iterator = instances.find(createdId);
                    if (iterator == instances.end())
                    {
                        continue;
                    }
                    static_cast<void>(DestroyInstance(iterator->second));
                    instances.erase(iterator);
                }
                return Result<void>::Failure(created.GetError());
            }

            instances.emplace(entry.entityId, std::move(created).Value());
            createdIds.push_back(entry.entityId);
        }

        scriptWriteTimes[handle] = currentWriteTime;
        return Result<void>::Success();
    }

    [[nodiscard]] Result<void> ReloadChangedScripts()
    {
        auto desiredResult = BuildDesiredInstances();
        if (!desiredResult)
        {
            return Result<void>::Failure(desiredResult.GetError());
        }

        const std::vector<DesiredInstance> desired =
            std::move(desiredResult).Value();
        const std::vector<AssetHandle> handles =
            DesiredScriptHandles(desired);

        for (const AssetHandle handle : handles)
        {
            auto currentTime = assets.GetLastWriteTime(handle);
            if (!currentTime)
            {
                return Result<void>::Failure(currentTime.GetError());
            }

            const auto tracked = scriptWriteTimes.find(handle);
            if (tracked == scriptWriteTimes.end())
            {
                // New script components have not loaded source yet. Establish
                // a baseline; the following Reconcile/Update will load the
                // current file contents.
                scriptWriteTimes.emplace(handle, currentTime.Value());
                continue;
            }

            if (tracked->second == currentTime.Value())
            {
                continue;
            }

            auto reloaded = ReloadScript(
                handle,
                desired,
                currentTime.Value());
            if (!reloaded)
            {
                return reloaded;
            }
        }

        return Result<void>::Success();
    }

    [[nodiscard]] Result<void> UpdateInstances(TimeStep timeStep)
    {
        for (const UUID& entityId : SortedInstanceIds())
        {
            const auto iterator = instances.find(entityId);
            if (iterator == instances.end())
            {
                continue;
            }
            auto updated = CallCallback(
                iterator->second,
                "OnUpdate",
                timeStep.GetSeconds());
            if (!updated)
            {
                return updated;
            }
        }
        return Result<void>::Success();
    }

    [[nodiscard]] Result<void> StopInstances()
    {
        std::optional<Error> firstError;
        for (const UUID& entityId : SortedInstanceIds())
        {
            auto iterator = instances.find(entityId);
            if (iterator == instances.end())
            {
                continue;
            }
            auto destroyed = DestroyInstance(iterator->second);
            if (!destroyed && !firstError.has_value())
            {
                firstError = destroyed.GetError();
            }
            instances.erase(iterator);
        }

        if (firstError.has_value())
        {
            return Result<void>::Failure(std::move(*firstError));
        }
        return Result<void>::Success();
    }

    void ForceReleaseAll() noexcept
    {
        if (virtualMachine == nullptr)
        {
            instances.clear();
            return;
        }

        lua_State* state = State();
        for (auto& [entityId, instance] : instances)
        {
            static_cast<void>(entityId);
            if (instance.tableRegistryRef != LUA_NOREF
                && instance.tableRegistryRef != LUA_REFNIL)
            {
                luaL_unref(state, LUA_REGISTRYINDEX, instance.tableRegistryRef);
                instance.tableRegistryRef = LUA_NOREF;
            }
        }
        instances.clear();
    }

    Scene& scene;
    AssetService& assets;
    const InputState& input;
    std::unique_ptr<LuaVirtualMachine> virtualMachine;
    BindingContext bindingContext;
    std::unordered_map<UUID, ScriptInstance, UUIDHash> instances;
    std::unordered_map<
        AssetHandle,
        std::filesystem::file_time_type,
        AssetHandleHash> scriptWriteTimes;
    bool running = false;
};

Result<std::unique_ptr<ScriptEngine>> ScriptEngine::Create(
    Scene& scene,
    AssetService& assets,
    const InputState& input)
{
    auto machine = LuaVirtualMachine::Create();
    if (!machine)
    {
        return Result<std::unique_ptr<ScriptEngine>>::Failure(
            machine.GetError());
    }

    auto impl = std::make_unique<Impl>(
        scene,
        assets,
        input,
        std::move(machine).Value());
    impl->InitializeBindings();

    return Result<std::unique_ptr<ScriptEngine>>::Success(
        std::unique_ptr<ScriptEngine>(
            new ScriptEngine(std::move(impl))));
}

ScriptEngine::ScriptEngine(std::unique_ptr<Impl> impl) noexcept
    : m_Impl(std::move(impl))
{
}

ScriptEngine::~ScriptEngine()
{
    if (m_Impl != nullptr)
    {
        m_Impl->ForceReleaseAll();
    }
}

Result<void> ScriptEngine::Start()
{
    if (m_Impl->running)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "ScriptEngine is already running.");
    }

    m_Impl->running = true;
    auto reconciled = m_Impl->Reconcile();
    if (!reconciled)
    {
        const Error startupError = reconciled.GetError();
        static_cast<void>(m_Impl->StopInstances());
        m_Impl->scriptWriteTimes.clear();
        m_Impl->running = false;
        return Result<void>::Failure(startupError);
    }

    auto captured = m_Impl->CaptureActiveWriteTimes();
    if (!captured)
    {
        const Error startupError = captured.GetError();
        static_cast<void>(m_Impl->StopInstances());
        m_Impl->scriptWriteTimes.clear();
        m_Impl->running = false;
        return Result<void>::Failure(startupError);
    }

    return Result<void>::Success();
}

Result<void> ScriptEngine::ReloadChangedScripts()
{
    if (!m_Impl->running)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "ScriptEngine must be started before ReloadChangedScripts.");
    }

    return m_Impl->ReloadChangedScripts();
}

Result<void> ScriptEngine::Update(TimeStep timeStep)
{
    if (!m_Impl->running)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "ScriptEngine must be started before Update.");
    }

    auto reconciled = m_Impl->Reconcile();
    if (!reconciled)
    {
        return reconciled;
    }
    return m_Impl->UpdateInstances(timeStep);
}

Result<void> ScriptEngine::Stop()
{
    if (!m_Impl->running)
    {
        return Result<void>::Success();
    }

    auto stopped = m_Impl->StopInstances();
    m_Impl->scriptWriteTimes.clear();
    m_Impl->running = false;
    return stopped;
}

bool ScriptEngine::IsRunning() const noexcept
{
    return m_Impl->running;
}

usize ScriptEngine::InstanceCount() const noexcept
{
    return m_Impl->instances.size();
}

} // namespace Janus
