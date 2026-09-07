#include "Animation/AnimatorComponent.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include <cmath>
#include <type_traits>

namespace Janus
{
Result<void> ValidateAnimator(const AnimatorComponent& value)
{
    if (!std::isfinite(value.speed) || value.speed < 0 || value.speed > 100 ||
        (value.enabled && !value.clip.id.IsValid()))
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Animator needs speed in [0,100] and an animation-clip reference when enabled.");
    return Result<void>::Success();
}
namespace
{
template <typename T> PropertyDescriptor Field(const char* name, T AnimatorComponent::* member)
{
    PropertyDescriptor property;
    property.id = MakePropertyId(std::string("Animator.") + name);
    property.name = property.serializedName = name;
    property.type = GetPropertyType(PropertyValue{T{}});
    if constexpr (std::is_same_v<T, AssetReferenceValue>)
        property.referenceConstraint = "animation-clip";
    property.getter = [member](const void* object)
    {
        return Result<PropertyValue>::Success(
            static_cast<const AnimatorComponent*>(object)->*member);
    };
    property.setter = [member](void* object, const PropertyValue& value)
    {
        auto next = *static_cast<AnimatorComponent*>(object);
        next.*member = std::get<T>(value);
        auto valid = ValidateAnimator(next);
        if (!valid)
            return valid;
        *static_cast<AnimatorComponent*>(object) = std::move(next);
        return Result<void>::Success();
    };
    return property;
}
} // namespace
Result<void> RegisterAnimationReflection(ReflectionRegistry& registry)
{
    return registry.RegisterComponent(ComponentDescriptor{
        MakeComponentTypeId("Animator"),
        "Animator",
        "Animator",
        true,
        true,
        {Field("clip", &AnimatorComponent::clip), Field("enabled", &AnimatorComponent::enabled),
         Field("playOnStart", &AnimatorComponent::playOnStart),
         Field("speed", &AnimatorComponent::speed)},
        [](const void* object)
        { return ValidateAnimator(*static_cast<const AnimatorComponent*>(object)); }});
}
} // namespace Janus
