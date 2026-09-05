#pragma once

#include "Core/Reflection/ReflectionRegistry.h"
#include "Protocol/JsonRpc.h"

namespace Janus::MCP
{

[[nodiscard]] Json BuildPropertySchema(
    const PropertyDescriptor& property);

[[nodiscard]] Json BuildComponentSchema(
    const ComponentDescriptor& component);

[[nodiscard]] Json BuildReflectionCatalogSchema(
    const ReflectionRegistry& registry);

[[nodiscard]] Json BuildPropertyMutationSchema(
    const ReflectionRegistry& registry);

} // namespace Janus::MCP
