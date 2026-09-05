#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Types.h"
#include "Core/UUID/UUID.h"

#include <compare>
#include <string>
#include <string_view>
#include <variant>

namespace Janus
{

struct ComponentTypeId
{
    u64 value = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return value != 0;
    }

    auto operator<=>(const ComponentTypeId&) const = default;
};

struct PropertyId
{
    u64 value = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return value != 0;
    }

    auto operator<=>(const PropertyId&) const = default;
};

[[nodiscard]] constexpr u64 HashReflectionName(
    std::string_view name) noexcept
{
    constexpr u64 offsetBasis = 14695981039346656037ull;
    constexpr u64 prime = 1099511628211ull;

    u64 hash = offsetBasis;
    for (const char value : name)
    {
        hash ^= static_cast<u8>(value);
        hash *= prime;
    }

    return hash;
}

[[nodiscard]] constexpr ComponentTypeId MakeComponentTypeId(
    std::string_view canonicalName) noexcept
{
    return ComponentTypeId{HashReflectionName(canonicalName)};
}

[[nodiscard]] constexpr PropertyId MakePropertyId(
    std::string_view canonicalName) noexcept
{
    return PropertyId{HashReflectionName(canonicalName)};
}

struct ColorValue
{
    f32 r = 1.0f;
    f32 g = 1.0f;
    f32 b = 1.0f;
    f32 a = 1.0f;
};

struct AssetReferenceValue
{
    UUID id;
};

enum class PropertyType
{
    Unknown = 0,
    Bool,
    Int32,
    Float32,
    String,
    Vector2,
    Color,
    AssetReference
};

using PropertyValue = std::variant<
    bool,
    i32,
    f32,
    std::string,
    Vector2,
    ColorValue,
    AssetReferenceValue>;

[[nodiscard]] inline PropertyType GetPropertyType(
    const PropertyValue& value) noexcept
{
    if (value.valueless_by_exception())
    {
        return PropertyType::Unknown;
    }

    switch (value.index())
    {
    case 0:
        return PropertyType::Bool;
    case 1:
        return PropertyType::Int32;
    case 2:
        return PropertyType::Float32;
    case 3:
        return PropertyType::String;
    case 4:
        return PropertyType::Vector2;
    case 5:
        return PropertyType::Color;
    case 6:
        return PropertyType::AssetReference;
    default:
        return PropertyType::Unknown;
    }
}

} // namespace Janus
