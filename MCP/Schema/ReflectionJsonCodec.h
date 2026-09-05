#pragma once

#include "Core/Error/Result.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Protocol/JsonRpc.h"

#include <string_view>

namespace Janus::MCP
{

[[nodiscard]] Result<Json> PropertyValueToMcpJson(
    const PropertyValue& value);

[[nodiscard]] Result<PropertyValue> McpJsonToPropertyValue(
    const Json& value,
    const PropertyDescriptor& property);

[[nodiscard]] std::string_view McpPropertyTypeName(
    PropertyType type) noexcept;

} // namespace Janus::MCP
