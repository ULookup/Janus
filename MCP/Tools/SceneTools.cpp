#include "Tools/SceneTools.h"

#include "Asset/AssetRegistry.h"
#include "Core/Command/CommandBus.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Schema/JsonSchema.h"
#include "Schema/ReflectionJsonCodec.h"
#include "Schema/ReflectionSchemaAdapter.h"
#include "Scene/Command/EntityCommands.h"
#include "Scene/Command/SceneCommands.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Janus::MCP
{
namespace
{

McpDispatchError InvalidParams(
    std::string message)
{
    return McpDispatchError{
        JsonRpcInvalidParams,
        std::move(message),
        nullptr};
}

Json UuidSchema()
{
    return Json{
        {"type", "string"},
        {"format", "uuid"}};
}

Json ToolResult(
    Json structured,
    bool isError = false)
{
    Json result = {
        {"content",
         Json::array(
             {Json{
                 {"type", "text"},
                 {"text", structured.dump()}})},
        {"structuredContent",
         structured}};

    if (isError)
    {
        result["isError"] = true;
    }

    return result;
}

Json ToolExecutionError(
    const Error& error)
{
    return ToolResult(
        Json{
            {"ok", false},
            {"error",
             Json{
                 {"code",
                  static_cast<i32>(
                      error.code)},
                 {"message",
                  error.message}}}},
        true);
}

bool AllowedKeys(
    const Json& arguments,
    std::initializer_list<std::string_view> allowed)
{
    if (!arguments.is_object())
    {
        return false;
    }

    for (auto it = arguments.begin();
         it != arguments.end();
         ++it)
    {
        bool found = false;
        for (const std::string_view key : allowed)
        {
            if (it.key() == key)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            return false;
        }
    }

    return true;
}

Result<UUID> ParseEntityUuid(
    const Json& arguments)
{
    const auto entityIt =
        arguments.find("entity");

    if (entityIt == arguments.end()
        || !entityIt->is_string())
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "MCP Scene tool requires string entity UUID.");
    }

    auto id =
        UUID::Parse(
            entityIt->get_ref<const std::string&>());
    if (!id)
    {
        return id;
    }

    if (!id.Value().IsValid())
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "MCP Scene entity UUID cannot be nil.");
    }

    return id;
}

Result<void> CheckWritable(
    const McpSceneToolContext& context)
{
    if (context.authoringReadOnly
        && context.authoringReadOnly())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Janus authoring state is currently read-only.");
    }

    return Result<void>::Success();
}

Result<void> ExecuteCommand(
    const McpSceneToolContext& context,
    std::unique_ptr<ICommand> command)
{
    auto writable =
        CheckWritable(context);
    if (!writable)
    {
        return writable;
    }

    auto executed =
        context.commands->Execute(
            std::move(command));
    if (!executed)
    {
        return executed;
    }

    if (context.markDirty)
    {
        context.markDirty();
    }

    return Result<void>::Success();
}

const ComponentDescriptor* ResolveComponent(
    const McpSceneToolContext& context,
    const Json& arguments)
{
    const auto componentIt =
        arguments.find("component");

    if (componentIt == arguments.end()
        || !componentIt->is_string())
    {
        return nullptr;
    }

    return context.reflection
        ->FindComponentBySerializedName(
            componentIt
                ->get_ref<const std::string&>());
}

Json ComponentSelectorSchema(
    const ReflectionRegistry& reflection,
    bool removableOnly)
{
    Json names =
        Json::array();

    std::vector<const ComponentDescriptor*> components =
        reflection.GetComponents();

    std::sort(
        components.begin(),
        components.end(),
        [](const ComponentDescriptor* left,
           const ComponentDescriptor* right)
        {
            return left->serializedName
                < right->serializedName;
        });

    for (const ComponentDescriptor* component : components)
    {
        if (!removableOnly
            || component->removable)
        {
            names.push_back(
                component->serializedName);
        }
    }

    return Json{
        {"type", "string"},
        {"enum", std::move(names)}};
}

Json ObjectInputSchema(
    Json properties,
    Json required)
{
    return Json{
        {"$schema",
         std::string{
             McpJsonSchema202012}},
        {"type", "object"},
        {"properties",
         std::move(properties)},
        {"required",
         std::move(required)},
        {"additionalProperties", false}};
}

Json BasicOutputSchema(
    Json properties,
    Json required)
{
    properties["ok"] =
        Json{{"const", true}};
    required.push_back("ok");

    Json success = {
        {"type", "object"},
        {"properties",
         std::move(properties)},
        {"required",
         std::move(required)},
        {"additionalProperties", false}};

    Json failure = {
        {"type", "object"},
        {"properties",
         Json{
             {"ok",
              Json{{"const", false}}},
             {"error",
              Json{
                  {"type", "object"},
                  {"properties",
                   Json{
                       {"code",
                        Json{{"type", "integer"}}},
                       {"message",
                        Json{{"type", "string"}}}}},
                  {"required",
                   Json::array(
                       {"code", "message"})},
                  {"additionalProperties", false}}}}},
        {"required",
         Json::array(
             {"ok", "error"})},
        {"additionalProperties", false}};

    return Json{
        {"$schema",
         std::string{
             McpJsonSchema202012}},
        {"oneOf",
         Json::array(
             {std::move(success),
              std::move(failure)})}};
}

Json SetPropertyInputSchema(
    const ReflectionRegistry& reflection)
{
    Json schema =
        BuildPropertyMutationSchema(
            reflection);

    for (Json& branch
         : schema["oneOf"])
    {
        branch["properties"]["entity"] =
            UuidSchema();
        branch["required"].push_back(
            "entity");
    }

    return schema;
}

McpToolDescriptor CreateEntityTool(
    McpSceneToolContext context)
{
    return McpToolDescriptor{
        "scene.create_entity",
        "Create Entity",
        "Create a root entity in the current Janus authoring Scene.",
        ObjectInputSchema(
            Json{
                {"name",
                 Json{
                     {"type", "string"}}}},
            Json::array()),
        BasicOutputSchema(
            Json{
                {"entity", UuidSchema()},
                {"name",
                 Json{{"type", "string"}}}},
            Json::array(
                {"entity", "name"})),
        Json{
            {"destructiveHint", false}},
        [context = std::move(context)](
            const Json& arguments,
            McpProtocolEra) -> McpDispatchResult
        {
            if (!AllowedKeys(
                    arguments,
                    {"name"}))
            {
                return InvalidParams(
                    "scene.create_entity accepts only name.");
            }

            std::string name =
                "Entity";

            if (const auto nameIt =
                    arguments.find("name");
                nameIt != arguments.end())
            {
                if (!nameIt->is_string())
                {
                    return InvalidParams(
                        "scene.create_entity name must be a string.");
                }

                name =
                    nameIt->get<std::string>();
            }

            const UUID entity =
                UUID::Random();

            auto executed =
                ExecuteCommand(
                    context,
                    std::make_unique<CreateEntityCommand>(
                        *context.scene,
                        entity,
                        name));

            if (!executed)
            {
                return ToolExecutionError(
                    executed.GetError());
            }

            return ToolResult(
                Json{
                    {"ok", true},
                    {"entity", entity.ToString()},
                    {"name", std::move(name)}});
        }};
}

McpToolDescriptor DeleteEntityTool(
    McpSceneToolContext context)
{
    return McpToolDescriptor{
        "scene.delete_entity",
        "Delete Entity",
        "Delete an entity subtree from the current Janus authoring Scene.",
        ObjectInputSchema(
            Json{
                {"entity", UuidSchema()}},
            Json::array({"entity"})),
        BasicOutputSchema(
            Json{
                {"entity", UuidSchema()}},
            Json::array({"entity"})),
        Json{
            {"destructiveHint", true}},
        [context = std::move(context)](
            const Json& arguments,
            McpProtocolEra) -> McpDispatchResult
        {
            if (!AllowedKeys(
                    arguments,
                    {"entity"}))
            {
                return InvalidParams(
                    "scene.delete_entity accepts only entity.");
            }

            auto entity =
                ParseEntityUuid(
                    arguments);
            if (!entity)
            {
                return InvalidParams(
                    entity.GetError().message);
            }

            SceneReflection reflection(
                *context.reflection,
                context.assets);

            auto executed =
                ExecuteCommand(
                    context,
                    std::make_unique<DeleteEntityCommand>(
                        *context.scene,
                        reflection,
                        entity.Value()));

            if (!executed)
            {
                return ToolExecutionError(
                    executed.GetError());
            }

            return ToolResult(
                Json{
                    {"ok", true},
                    {"entity",
                     entity.Value().ToString()}});
        }};
}

McpToolDescriptor RenameEntityTool(
    McpSceneToolContext context)
{
    return McpToolDescriptor{
        "scene.rename_entity",
        "Rename Entity",
        "Rename a persistent Janus authoring entity.",
        ObjectInputSchema(
            Json{
                {"entity", UuidSchema()},
                {"name",
                 Json{
                     {"type", "string"}}}},
            Json::array(
                {"entity", "name"})),
        BasicOutputSchema(
            Json{
                {"entity", UuidSchema()},
                {"name",
                 Json{{"type", "string"}}}},
            Json::array(
                {"entity", "name"})),
        Json::object(),
        [context = std::move(context)](
            const Json& arguments,
            McpProtocolEra) -> McpDispatchResult
        {
            if (!AllowedKeys(
                    arguments,
                    {"entity", "name"}))
            {
                return InvalidParams(
                    "scene.rename_entity accepts only entity and name.");
            }

            auto entity =
                ParseEntityUuid(
                    arguments);
            if (!entity)
            {
                return InvalidParams(
                    entity.GetError().message);
            }

            const auto nameIt =
                arguments.find("name");
            if (nameIt == arguments.end()
                || !nameIt->is_string())
            {
                return InvalidParams(
                    "scene.rename_entity requires string name.");
            }

            const std::string name =
                nameIt->get<std::string>();

            auto executed =
                ExecuteCommand(
                    context,
                    std::make_unique<RenameEntityCommand>(
                        *context.scene,
                        entity.Value(),
                        name));

            if (!executed)
            {
                return ToolExecutionError(
                    executed.GetError());
            }

            return ToolResult(
                Json{
                    {"ok", true},
                    {"entity",
                     entity.Value().ToString()},
                    {"name", name}});
        }};
}

McpToolDescriptor AddComponentTool(
    McpSceneToolContext context)
{
    return McpToolDescriptor{
        "scene.add_component",
        "Add Component",
        "Add a reflected removable component to an authoring entity.",
        ObjectInputSchema(
            Json{
                {"entity", UuidSchema()},
                {"component",
                 ComponentSelectorSchema(
                     *context.reflection,
                     true)}},
            Json::array(
                {"entity", "component"})),
        BasicOutputSchema(
            Json{
                {"entity", UuidSchema()},
                {"component",
                 Json{{"type", "string"}}}},
            Json::array(
                {"entity", "component"})),
        Json::object(),
        [context = std::move(context)](
            const Json& arguments,
            McpProtocolEra) -> McpDispatchResult
        {
            if (!AllowedKeys(
                    arguments,
                    {"entity", "component"}))
            {
                return InvalidParams(
                    "scene.add_component accepts only entity and component.");
            }

            auto entity =
                ParseEntityUuid(
                    arguments);
            if (!entity)
            {
                return InvalidParams(
                    entity.GetError().message);
            }

            const ComponentDescriptor* component =
                ResolveComponent(
                    context,
                    arguments);

            if (component == nullptr
                || !component->removable)
            {
                return InvalidParams(
                    "scene.add_component requires a known removable reflected component.");
            }

            SceneReflection reflection(
                *context.reflection,
                context.assets);

            auto executed =
                ExecuteCommand(
                    context,
                    std::make_unique<AddComponentCommand>(
                        *context.scene,
                        reflection,
                        entity.Value(),
                        component->id));

            if (!executed)
            {
                return ToolExecutionError(
                    executed.GetError());
            }

            return ToolResult(
                Json{
                    {"ok", true},
                    {"entity",
                     entity.Value().ToString()},
                    {"component",
                     component->serializedName}});
        }};
}

McpToolDescriptor RemoveComponentTool(
    McpSceneToolContext context)
{
    return McpToolDescriptor{
        "scene.remove_component",
        "Remove Component",
        "Remove a reflected removable component from an authoring entity.",
        ObjectInputSchema(
            Json{
                {"entity", UuidSchema()},
                {"component",
                 ComponentSelectorSchema(
                     *context.reflection,
                     true)}},
            Json::array(
                {"entity", "component"})),
        BasicOutputSchema(
            Json{
                {"entity", UuidSchema()},
                {"component",
                 Json{{"type", "string"}}}},
            Json::array(
                {"entity", "component"})),
        Json{
            {"destructiveHint", true}},
        [context = std::move(context)](
            const Json& arguments,
            McpProtocolEra) -> McpDispatchResult
        {
            if (!AllowedKeys(
                    arguments,
                    {"entity", "component"}))
            {
                return InvalidParams(
                    "scene.remove_component accepts only entity and component.");
            }

            auto entity =
                ParseEntityUuid(
                    arguments);
            if (!entity)
            {
                return InvalidParams(
                    entity.GetError().message);
            }

            const ComponentDescriptor* component =
                ResolveComponent(
                    context,
                    arguments);

            if (component == nullptr
                || !component->removable)
            {
                return InvalidParams(
                    "scene.remove_component requires a known removable reflected component.");
            }

            SceneReflection reflection(
                *context.reflection,
                context.assets);

            auto executed =
                ExecuteCommand(
                    context,
                    std::make_unique<RemoveComponentCommand>(
                        *context.scene,
                        reflection,
                        entity.Value(),
                        component->id));

            if (!executed)
            {
                return ToolExecutionError(
                    executed.GetError());
            }

            return ToolResult(
                Json{
                    {"ok", true},
                    {"entity",
                     entity.Value().ToString()},
                    {"component",
                     component->serializedName}});
        }};
}

McpToolDescriptor SetPropertyTool(
    McpSceneToolContext context)
{
    return McpToolDescriptor{
        "scene.set_component_property",
        "Set Component Property",
        "Set one reflected authoring property through Janus CommandBus.",
        SetPropertyInputSchema(
            *context.reflection),
        BasicOutputSchema(
            Json{
                {"entity", UuidSchema()},
                {"component",
                 Json{{"type", "string"}}},
                {"property",
                 Json{{"type", "string"}}}},
            Json::array(
                {"entity", "component", "property"})),
        Json::object(),
        [context = std::move(context)](
            const Json& arguments,
            McpProtocolEra) -> McpDispatchResult
        {
            if (!AllowedKeys(
                    arguments,
                    {"entity", "component", "property", "value"}))
            {
                return InvalidParams(
                    "scene.set_component_property received unknown arguments.");
            }

            auto entity =
                ParseEntityUuid(
                    arguments);
            if (!entity)
            {
                return InvalidParams(
                    entity.GetError().message);
            }

            const ComponentDescriptor* component =
                ResolveComponent(
                    context,
                    arguments);
            if (component == nullptr)
            {
                return InvalidParams(
                    "Unknown reflected component.");
            }

            const auto propertyIt =
                arguments.find("property");
            const auto valueIt =
                arguments.find("value");

            if (propertyIt == arguments.end()
                || !propertyIt->is_string()
                || valueIt == arguments.end())
            {
                return InvalidParams(
                    "scene.set_component_property requires property and value.");
            }

            const PropertyDescriptor* property =
                component->FindPropertyBySerializedName(
                    propertyIt
                        ->get_ref<const std::string&>());

            if (property == nullptr
                || !property->editable)
            {
                return InvalidParams(
                    "Unknown or non-editable reflected property.");
            }

            auto value =
                McpJsonToPropertyValue(
                    *valueIt,
                    *property);
            if (!value)
            {
                return InvalidParams(
                    value.GetError().message);
            }

            SceneReflection reflection(
                *context.reflection,
                context.assets);

            auto executed =
                ExecuteCommand(
                    context,
                    std::make_unique<SetPropertyCommand>(
                        *context.scene,
                        reflection,
                        entity.Value(),
                        component->id,
                        property->id,
                        std::move(value).Value()));

            if (!executed)
            {
                return ToolExecutionError(
                    executed.GetError());
            }

            return ToolResult(
                Json{
                    {"ok", true},
                    {"entity",
                     entity.Value().ToString()},
                    {"component",
                     component->serializedName},
                    {"property",
                     property->serializedName}});
        }};
}

McpToolDescriptor SaveSceneTool(
    McpSceneToolContext context)
{
    return McpToolDescriptor{
        "scene.save",
        "Save Scene",
        "Persist the current Janus authoring Scene through the host save capability.",
        ObjectInputSchema(
            Json::object(),
            Json::array()),
        BasicOutputSchema(
            Json::object(),
            Json::array()),
        Json{
            {"destructiveHint", false}},
        [context = std::move(context)](
            const Json& arguments,
            McpProtocolEra) -> McpDispatchResult
        {
            if (!AllowedKeys(
                    arguments,
                    {}))
            {
                return InvalidParams(
                    "scene.save accepts no arguments.");
            }

            auto writable =
                CheckWritable(
                    context);
            if (!writable)
            {
                return ToolExecutionError(
                    writable.GetError());
            }

            auto saved =
                context.saveCurrentScene();
            if (!saved)
            {
                return ToolExecutionError(
                    saved.GetError());
            }

            return ToolResult(
                Json{
                    {"ok", true}});
        }};
}

} // namespace

Result<void> RegisterSceneTools(
    ToolRegistry& registry,
    McpSceneToolContext context)
{
    if (context.scene == nullptr
        || context.reflection == nullptr
        || context.commands == nullptr
        || context.assets == nullptr
        || !context.saveCurrentScene)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP Scene tools require Scene, Reflection, CommandBus, AssetRegistry, and save callback.");
    }

    auto result =
        registry.RegisterTool(
            CreateEntityTool(context));
    if (!result)
    {
        return result;
    }

    result =
        registry.RegisterTool(
            DeleteEntityTool(context));
    if (!result)
    {
        return result;
    }

    result =
        registry.RegisterTool(
            RenameEntityTool(context));
    if (!result)
    {
        return result;
    }

    result =
        registry.RegisterTool(
            AddComponentTool(context));
    if (!result)
    {
        return result;
    }

    result =
        registry.RegisterTool(
            RemoveComponentTool(context));
    if (!result)
    {
        return result;
    }

    result =
        registry.RegisterTool(
            SetPropertyTool(context));
    if (!result)
    {
        return result;
    }

    return registry.RegisterTool(
        SaveSceneTool(
            std::move(context)));
}

} // namespace Janus::MCP
