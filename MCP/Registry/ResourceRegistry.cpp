#include "Registry/ResourceRegistry.h"

#include <algorithm>
#include <charconv>
#include <optional>
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

bool ValidateUriTemplate(
    std::string_view uriTemplate)
{
    bool hasVariable = false;
    usize index = 0;

    while (index < uriTemplate.size())
    {
        const usize open =
            uriTemplate.find('{', index);

        if (open == std::string_view::npos)
        {
            break;
        }

        const usize close =
            uriTemplate.find('}', open + 1);

        if (close == std::string_view::npos
            || close == open + 1)
        {
            return false;
        }

        if (uriTemplate.find('{', open + 1)
            < close)
        {
            return false;
        }

        hasVariable = true;
        index = close + 1;
    }

    return hasVariable
        && uriTemplate.find('}', index)
            == std::string_view::npos;
}

bool MatchUriTemplate(
    std::string_view uriTemplate,
    std::string_view uri)
{
    usize templateOffset = 0;
    usize uriOffset = 0;

    while (templateOffset < uriTemplate.size())
    {
        const usize open =
            uriTemplate.find(
                '{',
                templateOffset);

        if (open == std::string_view::npos)
        {
            return uri.substr(uriOffset)
                == uriTemplate.substr(templateOffset);
        }

        const std::string_view literal =
            uriTemplate.substr(
                templateOffset,
                open - templateOffset);

        if (!uri.substr(uriOffset).starts_with(literal))
        {
            return false;
        }

        uriOffset += literal.size();

        const usize close =
            uriTemplate.find(
                '}',
                open + 1);
        if (close == std::string_view::npos)
        {
            return false;
        }

        const usize nextOpen =
            uriTemplate.find(
                '{',
                close + 1);

        const std::string_view nextLiteral =
            nextOpen == std::string_view::npos
                ? uriTemplate.substr(close + 1)
                : uriTemplate.substr(
                      close + 1,
                      nextOpen - (close + 1));

        if (nextLiteral.empty())
        {
            if (nextOpen == std::string_view::npos)
            {
                return uriOffset < uri.size();
            }

            return false;
        }

        const usize nextPosition =
            uri.find(
                nextLiteral,
                uriOffset);

        if (nextPosition == std::string_view::npos
            || nextPosition == uriOffset)
        {
            return false;
        }

        uriOffset =
            nextPosition;
        templateOffset =
            close + 1;
    }

    return uriOffset == uri.size();
}

Json EncodeResource(
    const McpResourceDescriptor& resource)
{
    Json encoded = {
        {"uri", resource.uri},
        {"name", resource.name}};

    if (!resource.title.empty())
    {
        encoded["title"] =
            resource.title;
    }

    if (!resource.description.empty())
    {
        encoded["description"] =
            resource.description;
    }

    if (!resource.mimeType.empty())
    {
        encoded["mimeType"] =
            resource.mimeType;
    }

    return encoded;
}

Json EncodeTemplate(
    const McpResourceTemplateDescriptor& resource)
{
    Json encoded = {
        {"uriTemplate", resource.uriTemplate},
        {"name", resource.name}};

    if (!resource.title.empty())
    {
        encoded["title"] =
            resource.title;
    }

    if (!resource.description.empty())
    {
        encoded["description"] =
            resource.description;
    }

    if (!resource.mimeType.empty())
    {
        encoded["mimeType"] =
            resource.mimeType;
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

template <typename Map, typename Encoder>
McpDispatchResult EncodeList(
    const Map& entries,
    const Json& params,
    std::string_view key,
    McpProtocolEra era,
    Encoder encoder)
{
    usize offset = 0;
    if (const auto cursorError =
            ParseCursor(
                params,
                entries.size(),
                offset);
        cursorError.has_value())
    {
        return *cursorError;
    }

    Json result = {
        {std::string{key}, Json::array()}};

    const usize end =
        std::min(
            offset + ListPageSize,
            entries.size());

    usize index = 0;
    for (const auto& [name, descriptor] : entries)
    {
        (void)name;

        if (index >= offset
            && index < end)
        {
            result[std::string{key}].push_back(
                encoder(descriptor));
        }

        ++index;
        if (index >= end)
        {
            break;
        }
    }

    if (end < entries.size())
    {
        result["nextCursor"] =
            std::to_string(end);
    }

    AddModernCacheHints(
        result,
        era);

    return result;
}

} // namespace

Result<void> ResourceRegistry::RegisterResource(
    McpResourceDescriptor descriptor)
{
    if (descriptor.uri.empty()
        || descriptor.name.empty())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP resource requires non-empty URI and name.");
    }

    if (!descriptor.handler)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP resource requires a read handler.");
    }

    if (m_Resources.contains(descriptor.uri))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP resource URI is already registered.");
    }

    const std::string uri =
        descriptor.uri;

    m_Resources.emplace(
        uri,
        std::move(descriptor));

    return Result<void>::Success();
}

Result<void> ResourceRegistry::RegisterTemplate(
    McpResourceTemplateDescriptor descriptor)
{
    if (descriptor.uriTemplate.empty()
        || descriptor.name.empty())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP resource template requires non-empty URI template and name.");
    }

    if (!ValidateUriTemplate(
            descriptor.uriTemplate))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP resource template must contain valid named placeholders.");
    }

    if (!descriptor.handler)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP resource template requires a read handler.");
    }

    if (m_Templates.contains(
            descriptor.uriTemplate))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP resource template is already registered.");
    }

    const std::string uriTemplate =
        descriptor.uriTemplate;

    m_Templates.emplace(
        uriTemplate,
        std::move(descriptor));

    return Result<void>::Success();
}

const McpResourceDescriptor* ResourceRegistry::FindResource(
    std::string_view uri) const noexcept
{
    const auto found =
        m_Resources.find(
            std::string{uri});

    return found == m_Resources.end()
        ? nullptr
        : &found->second;
}

const McpResourceTemplateDescriptor* ResourceRegistry::FindTemplate(
    std::string_view uriTemplate) const noexcept
{
    const auto found =
        m_Templates.find(
            std::string{uriTemplate});

    return found == m_Templates.end()
        ? nullptr
        : &found->second;
}

const McpResourceTemplateDescriptor* ResourceRegistry::MatchTemplate(
    std::string_view uri) const noexcept
{
    for (const auto& [uriTemplate, descriptor] : m_Templates)
    {
        if (MatchUriTemplate(
                uriTemplate,
                uri))
        {
            return &descriptor;
        }
    }

    return nullptr;
}

std::vector<const McpResourceDescriptor*> ResourceRegistry::GetResources()
    const
{
    std::vector<const McpResourceDescriptor*> resources;
    resources.reserve(m_Resources.size());

    for (const auto& [uri, descriptor] : m_Resources)
    {
        (void)uri;
        resources.push_back(&descriptor);
    }

    return resources;
}

std::vector<const McpResourceTemplateDescriptor*>
ResourceRegistry::GetTemplates() const
{
    std::vector<const McpResourceTemplateDescriptor*> templates;
    templates.reserve(m_Templates.size());

    for (const auto& [uriTemplate, descriptor] : m_Templates)
    {
        (void)uriTemplate;
        templates.push_back(&descriptor);
    }

    return templates;
}

McpDispatchResult ResourceRegistry::HandleList(
    const Json& params,
    McpProtocolEra era) const
{
    if (!params.is_object())
    {
        return InvalidParams(
            "resources/list params must be an object.");
    }

    return EncodeList(
        m_Resources,
        params,
        "resources",
        era,
        EncodeResource);
}

McpDispatchResult ResourceRegistry::HandleTemplatesList(
    const Json& params,
    McpProtocolEra era) const
{
    if (!params.is_object())
    {
        return InvalidParams(
            "resources/templates/list params must be an object.");
    }

    return EncodeList(
        m_Templates,
        params,
        "resourceTemplates",
        era,
        EncodeTemplate);
}

McpDispatchResult ResourceRegistry::HandleRead(
    const Json& params,
    McpProtocolEra era) const
{
    if (!params.is_object())
    {
        return InvalidParams(
            "resources/read params must be an object.");
    }

    const auto uriIt =
        params.find("uri");

    if (uriIt == params.end()
        || !uriIt->is_string()
        || uriIt->get_ref<const std::string&>().empty())
    {
        return InvalidParams(
            "resources/read requires a non-empty URI.");
    }

    const std::string& uri =
        uriIt->get_ref<const std::string&>();

    const McpResourceDescriptor* resource =
        FindResource(uri);

    McpDispatchResult result =
        resource != nullptr
            ? resource->handler(uri, era)
            : McpDispatchResult{
                  InvalidParams(
                      "Requested MCP resource is not registered.")};

    if (resource == nullptr)
    {
        if (const auto* resourceTemplate =
                MatchTemplate(uri);
            resourceTemplate != nullptr)
        {
            result =
                resourceTemplate->handler(
                    uri,
                    era);
        }
    }

    if (auto* value =
            std::get_if<Json>(&result);
        value != nullptr)
    {
        if (!value->is_object())
        {
            return McpDispatchError{
                JsonRpcInternalError,
                "MCP resource handler returned a non-object result.",
                nullptr};
        }

        AddModernCacheHints(
            *value,
            era);
    }

    return result;
}

usize ResourceRegistry::GetResourceCount() const noexcept
{
    return m_Resources.size();
}

usize ResourceRegistry::GetTemplateCount() const noexcept
{
    return m_Templates.size();
}

} // namespace Janus::MCP
