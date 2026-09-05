#pragma once

#include "Core/Error/Result.h"
#include "Core/Reflection/ReflectionTypes.h"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Janus
{

using PropertyGetter =
    std::function<Result<PropertyValue>(const void*)>;
using PropertySetter =
    std::function<Result<void>(void*, const PropertyValue&)>;

struct PropertyDescriptor
{
    PropertyId id;
    std::string name;
    std::string serializedName;
    PropertyType type = PropertyType::Unknown;

    bool visible = true;
    bool editable = true;
    bool serializable = true;

    std::string referenceConstraint;

    PropertyGetter getter;
    PropertySetter setter;

    [[nodiscard]] Result<PropertyValue> Get(
        const void* component) const;

    [[nodiscard]] Result<void> Set(
        void* component,
        const PropertyValue& value) const;
};

struct ComponentDescriptor
{
    ComponentTypeId id;
    std::string name;
    std::string serializedName;

    bool removable = true;
    bool serializable = true;

    std::vector<PropertyDescriptor> properties;

    [[nodiscard]] const PropertyDescriptor* FindProperty(
        PropertyId propertyId) const noexcept;

    [[nodiscard]] const PropertyDescriptor* FindProperty(
        std::string_view propertyName) const noexcept;

    [[nodiscard]] const PropertyDescriptor* FindPropertyBySerializedName(
        std::string_view serializedName) const noexcept;
};

class ReflectionRegistry final
{
public:
    [[nodiscard]] Result<void> RegisterComponent(
        ComponentDescriptor descriptor);

    [[nodiscard]] const ComponentDescriptor* FindComponent(
        ComponentTypeId componentId) const noexcept;

    [[nodiscard]] const ComponentDescriptor* FindComponent(
        std::string_view componentName) const noexcept;

    [[nodiscard]] const ComponentDescriptor* FindComponentBySerializedName(
        std::string_view serializedName) const noexcept;

    [[nodiscard]] std::vector<const ComponentDescriptor*> GetComponents()
        const;

    [[nodiscard]] usize GetComponentCount() const noexcept;

private:
    std::map<std::string, ComponentDescriptor> m_Components;
    std::unordered_map<u64, std::string> m_ComponentNamesById;
    std::map<std::string, std::string> m_ComponentNamesBySerializedName;
};

} // namespace Janus
