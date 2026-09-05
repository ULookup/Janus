#include "Core/Reflection/ReflectionRegistry.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace Janus
{
namespace
{

Result<void> ValidateProperty(
    const PropertyDescriptor& property)
{
    if (!property.id.IsValid())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection property requires a valid PropertyId.");
    }

    if (property.name.empty() || property.serializedName.empty())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection property names cannot be empty.");
    }

    if (property.type == PropertyType::Unknown)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection property type cannot be Unknown.");
    }

    if (!property.getter)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection property requires a getter.");
    }

    if ((property.editable || property.serializable)
        && !property.setter)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Editable or serializable reflection property requires a setter.");
    }

    return Result<void>::Success();
}

Result<void> ValidateComponent(
    ComponentDescriptor& descriptor)
{
    if (!descriptor.id.IsValid())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection component requires a valid ComponentTypeId.");
    }

    if (descriptor.name.empty() || descriptor.serializedName.empty())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection component names cannot be empty.");
    }

    std::unordered_set<u64> propertyIds;
    std::unordered_set<std::string> propertyNames;
    std::unordered_set<std::string> serializedNames;

    for (const PropertyDescriptor& property : descriptor.properties)
    {
        auto valid = ValidateProperty(property);
        if (!valid)
        {
            return valid;
        }

        if (!propertyIds.insert(property.id.value).second)
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "Reflection component contains duplicate PropertyId values.");
        }

        if (!propertyNames.insert(property.name).second)
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "Reflection component contains duplicate property names.");
        }

        if (!serializedNames.insert(property.serializedName).second)
        {
            return Result<void>::Failure(
                ErrorCode::InvalidArgument,
                "Reflection component contains duplicate serialized property names.");
        }
    }

    std::sort(
        descriptor.properties.begin(),
        descriptor.properties.end(),
        [](const PropertyDescriptor& left, const PropertyDescriptor& right)
        {
            return left.name < right.name;
        });

    return Result<void>::Success();
}

} // namespace

Result<PropertyValue> PropertyDescriptor::Get(
    const void* component) const
{
    if (component == nullptr)
    {
        return Result<PropertyValue>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection property getter requires a component instance.");
    }

    if (!getter)
    {
        return Result<PropertyValue>::Failure(
            ErrorCode::InvalidState,
            "Reflection property getter is unavailable.");
    }

    auto value = getter(component);
    if (!value)
    {
        return value;
    }

    if (GetPropertyType(value.Value()) != type)
    {
        return Result<PropertyValue>::Failure(
            ErrorCode::InvalidState,
            "Reflection property getter returned a mismatched PropertyValue type.");
    }

    return value;
}

Result<void> PropertyDescriptor::Set(
    void* component,
    const PropertyValue& value) const
{
    if (component == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection property setter requires a component instance.");
    }

    if (!setter)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Reflection property setter is unavailable.");
    }

    if (GetPropertyType(value) != type)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection property setter received a mismatched PropertyValue type.");
    }

    return setter(component, value);
}

Result<void> ComponentDescriptor::Validate(
    const void* component) const
{
    if (component == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection component validation requires an instance.");
    }

    if (!validator)
    {
        return Result<void>::Success();
    }

    return validator(component);
}

const PropertyDescriptor* ComponentDescriptor::FindProperty(
    PropertyId propertyId) const noexcept
{
    const auto found = std::find_if(
        properties.begin(),
        properties.end(),
        [propertyId](const PropertyDescriptor& property)
        {
            return property.id == propertyId;
        });

    return found == properties.end()
        ? nullptr
        : &*found;
}

const PropertyDescriptor* ComponentDescriptor::FindProperty(
    std::string_view propertyName) const noexcept
{
    const auto found = std::find_if(
        properties.begin(),
        properties.end(),
        [propertyName](const PropertyDescriptor& property)
        {
            return property.name == propertyName;
        });

    return found == properties.end()
        ? nullptr
        : &*found;
}

const PropertyDescriptor* ComponentDescriptor::FindPropertyBySerializedName(
    std::string_view serializedName) const noexcept
{
    const auto found = std::find_if(
        properties.begin(),
        properties.end(),
        [serializedName](const PropertyDescriptor& property)
        {
            return property.serializedName == serializedName;
        });

    return found == properties.end()
        ? nullptr
        : &*found;
}

Result<void> ReflectionRegistry::RegisterComponent(
    ComponentDescriptor descriptor)
{
    auto valid = ValidateComponent(descriptor);
    if (!valid)
    {
        return valid;
    }

    if (m_Components.contains(descriptor.name))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection component name is already registered.");
    }

    const auto idFound =
        m_ComponentNamesById.find(descriptor.id.value);
    if (idFound != m_ComponentNamesById.end())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection ComponentTypeId is already registered.");
    }

    if (m_ComponentNamesBySerializedName.contains(
            descriptor.serializedName))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Reflection serialized component name is already registered.");
    }

    const std::string name = descriptor.name;
    const std::string serializedName = descriptor.serializedName;
    const u64 id = descriptor.id.value;

    m_Components.emplace(name, std::move(descriptor));
    m_ComponentNamesById.emplace(id, name);
    m_ComponentNamesBySerializedName.emplace(serializedName, name);

    return Result<void>::Success();
}

const ComponentDescriptor* ReflectionRegistry::FindComponent(
    ComponentTypeId componentId) const noexcept
{
    const auto name = m_ComponentNamesById.find(componentId.value);
    if (name == m_ComponentNamesById.end())
    {
        return nullptr;
    }

    const auto component = m_Components.find(name->second);
    return component == m_Components.end()
        ? nullptr
        : &component->second;
}

const ComponentDescriptor* ReflectionRegistry::FindComponent(
    std::string_view componentName) const noexcept
{
    const auto component =
        m_Components.find(std::string(componentName));
    return component == m_Components.end()
        ? nullptr
        : &component->second;
}

const ComponentDescriptor* ReflectionRegistry::FindComponentBySerializedName(
    std::string_view serializedName) const noexcept
{
    const auto name =
        m_ComponentNamesBySerializedName.find(
            std::string(serializedName));
    if (name == m_ComponentNamesBySerializedName.end())
    {
        return nullptr;
    }

    const auto component = m_Components.find(name->second);
    return component == m_Components.end()
        ? nullptr
        : &component->second;
}

std::vector<const ComponentDescriptor*> ReflectionRegistry::GetComponents()
    const
{
    std::vector<const ComponentDescriptor*> components;
    components.reserve(m_Components.size());

    for (const auto& [name, descriptor] : m_Components)
    {
        (void)name;
        components.push_back(&descriptor);
    }

    return components;
}

usize ReflectionRegistry::GetComponentCount() const noexcept
{
    return m_Components.size();
}

} // namespace Janus
