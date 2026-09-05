#include "Registry/ToolRegistry.h"

#include <algorithm>
#include <charconv>
#include <utility>

namespace Janus::MCP
{
namespace
{

inline constexpr usize ListPageSize = 64;

McpDispatchError InvalidParams(std::string message)
{
    return McpDispatchError{
        JsonRpcInvalidParams,
        std::move(message),
        nullptr};
}

Result<void> NormalizeSchema(
    Json& schema,
    bool requireObjectRoot)
{
    if (!schema.is_object())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP JSON Schema must be an object.");
    }

    const auto dialectIt =
        schema.find("$schema");

    if (dialectIt == schema.end())
    {
        schema["$schema"] =
            std::string{McpJsonSchema202012};
    }
    else if (!dialectIt->is_string()
        || dialectIt->get_ref<const std::string&>()
            != McpJsonSchema202012)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP schema must declare JSON Schema 2020-12.");
    }

    if (requireObjectRoot)
    {
        const auto typeIt =
            schema.find("type");

        if (typeIt == schema.end()
            || !typeIt->is_string()
            || typeIt->get_ref<const std::string&>()
                != "object")
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "MCP tool input schema root must be type object.");
        }
    }

    return Result<void>::Success();
}

std::optional<McpDispatchError> ParseCursor(
    const Json& params,
    usize count,
    usize& offset)
{
    offset = 0;

    const auto cursorIt =
        params.find("cursor");
    if (cursorIt == params.end())
    {
        return std::nullopt;
    }

    if (!cursorIt->is_string())
    {
        return InvalidParams(
            "MCP list cursor must be a string.");
    }

    const std::string& cursor =
        cursorIt->get_ref<const std::string&>();

    if (cursor.empty())
    {
        return InvalidParams(
            "MCP list cursor cannot be empty.");
    }

    usize parsed = 0;
    const auto [end, error] =
        std::from_chars(
            cursor.data(),
            cursor.data() + cursor.size(),
            parsed);

    if (error != std::errc{}
        || end != cursor.data() + cursor.size()
        || parsed > count)
    {
        return InvalidParams(
            "MCP list cursor is invalid.");
    }

    offset = parsed;
    return std::nullopt;
}

Json EncodeTool(
    const McpToolDescriptor& tool)
{
    Json encoded = {
        {"name", tool.name},
        {"inputSchema", tool.inputSchema}};

    if (!tool.title.empty())
    {
        encoded["title"] =
            tool.title;
    }

    if (!tool.description.empty())
    {
        encoded["description"] =
            tool.description;
    }

    if (tool.outputSchema.has_value())
    {
        encoded["outputSchema"] =
            *tool.outputSchema;
    }

    if (tool.annotations.is_object()
        && !tool.annotations.empty())
    {
        encoded["annotations"] =
            tool.annotations;
    }

    return encoded;
}

void AddModernCacheHints(
    Json& result,
    McpProtocolEra era)
{
    if (era != McpProtocolEra::Modern2026)
    {
        return;
    }

    result["ttlMs"] = 0;
    result["cacheScope"] = "private";
}

} // namespace

Result<void> ToolRegistry::RegisterTool(
    McpToolDescriptor descriptor)
{
    if (descriptor.name.empty())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP tool name cannot be empty.");
    }

    if (!descriptor.handler)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP tool requires a handler.");
    }

    auto inputSchema =
        NormalizeSchema(
            descriptor.inputSchema,
            true);
    if (!inputSchema)
    {
        return inputSchema;
    }

    if (descriptor.outputSchema.has_value())
    {
        auto outputSchema =
            NormalizeSchema(
                *descriptor.outputSchema,
                false);
        if (!outputSchema)
        {
            return outputSchema;
        }
    }

    if (!descriptor.annotations.is_object())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP tool annotations must be an object.");
    }

    if (m_Tools.contains(descriptor.name))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP tool name is already registered.");
    }

    const std::string name =
        descriptor.name;

    m_Tools.emplace(
        name,
        std::move(descriptor));

    return Result<void>::Success();
}

const McpToolDescriptor* ToolRegistry::FindTool(
    std::string_view name) const noexcept
{
    const auto found =
        m_Tools.find(std::string{name});

    return found == m_Tools.end()
        ? nullptr
        : &found->second;
}

std::vector<const McpToolDescriptor*> ToolRegistry::GetTools()
    const
{
    std::vector<const McpToolDescriptor*> tools;
    tools.reserve(m_Tools.size());

    for (const auto& [name, descriptor] : m_Tools)
    {
        (void)name;
        tools.push_back(&descriptor);
    }

    return tools;
}

McpDispatchResult ToolRegistry::HandleList(
    const Json& params,
    McpProtocolEra era) const
{
    if (!params.is_object())
    {
        return InvalidParams(
            "tools/list params must be an object.");
    }

    usize offset = 0;
    if (const auto cursorError =
            ParseCursor(
                params,
                m_Tools.size(),
                offset);
        cursorError.has_value())
    {
        return *cursorError;
    }

    Json result = {
        {"tools", Json::array()}};

    const usize end =
        std::min(
            offset + ListPageSize,
            m_Tools.size());

    usize index = 0;
    for (const auto& [name, descriptor] : m_Tools)
    {
        (void)name;

        if (index >= offset
            && index < end)
        {
            result["tools"].push_back(
                EncodeTool(descriptor));
        }

        ++index;
        if (index >= end)
        {
            break;
        }
    }

    if (end < m_Tools.size())
    {
        result["nextCursor"] =
            std::to_string(end);
    }

    AddModernCacheHints(
        result,
        era);

    return result;
}

McpDispatchResult ToolRegistry::HandleCall(
    const Json& params,
    McpProtocolEra era) const
{
    if (!params.is_object())
    {
        return InvalidParams(
            "tools/call params must be an object.");
    }

    const auto nameIt =
        params.find("name");

    if (nameIt == params.end()
        || !nameIt->is_string()
        || nameIt->get_ref<const std::string&>().empty())
    {
        return InvalidParams(
            "tools/call requires a non-empty tool name.");
    }

    const auto* descriptor =
        FindTool(
            nameIt->get_ref<const std::string&>());

    if (descriptor == nullptr)
    {
        return InvalidParams(
            "Requested MCP tool is not registered.");
    }

    Json arguments =
        Json::object();

    if (const auto argumentsIt =
            params.find("arguments");
        argumentsIt != params.end())
    {
        if (!argumentsIt->is_object())
        {
            return InvalidParams(
                "tools/call arguments must be an object.");
        }

        arguments =
            *argumentsIt;
    }

    return descriptor->handler(
        arguments,
        era);
}

usize ToolRegistry::GetToolCount() const noexcept
{
    return m_Tools.size();
}

} // namespace Janus::MCP
