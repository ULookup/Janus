#pragma once

#include "Core/Error/Result.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/UUID/UUID.h"

#include <vector>

namespace Janus
{

class Scene;

namespace Editor
{

struct InspectorPropertyModel
{
    const PropertyDescriptor* descriptor = nullptr;
    PropertyValue value;
};

struct InspectorComponentModel
{
    const ComponentDescriptor* descriptor = nullptr;
    bool present = false;
    std::vector<InspectorPropertyModel> properties;
};

[[nodiscard]] Result<std::vector<InspectorComponentModel>>
BuildInspectorModel(
    const Scene& scene,
    UUID entity,
    const ReflectionRegistry& registry);

} // namespace Editor
} // namespace Janus
