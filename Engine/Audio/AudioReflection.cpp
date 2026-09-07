#include "Audio/AudioSourceComponent.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include <cmath>
#include <type_traits>

namespace Janus
{
Result<void> ValidateAudioSource(const AudioSourceComponent& value)
{
    if (!std::isfinite(value.volume) || value.volume < 0 || value.volume > 1 ||
        (value.enabled && !value.clip.id.IsValid()))
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "AudioSource needs volume in [0,1] and an audio-clip reference when enabled.");
    return Result<void>::Success();
}
namespace
{
template <typename T> PropertyDescriptor Field(const char* name, T AudioSourceComponent::* member)
{
    PropertyDescriptor property;
    property.id = MakePropertyId(std::string("AudioSource.") + name);
    property.name = property.serializedName = name;
    property.type = GetPropertyType(PropertyValue{T{}});
    if constexpr (std::is_same_v<T, AssetReferenceValue>)
        property.referenceConstraint = "audio-clip";
    property.getter = [member](const void* object)
    {
        return Result<PropertyValue>::Success(
            static_cast<const AudioSourceComponent*>(object)->*member);
    };
    property.setter = [member](void* object, const PropertyValue& value)
    {
        auto next = *static_cast<AudioSourceComponent*>(object);
        next.*member = std::get<T>(value);
        auto valid = ValidateAudioSource(next);
        if (!valid)
            return valid;
        *static_cast<AudioSourceComponent*>(object) = std::move(next);
        return Result<void>::Success();
    };
    return property;
}
} // namespace
Result<void> RegisterAudioReflection(ReflectionRegistry& registry)
{
    return registry.RegisterComponent(ComponentDescriptor{
        MakeComponentTypeId("AudioSource"),
        "AudioSource",
        "AudioSource",
        true,
        true,
        {Field("clip", &AudioSourceComponent::clip),
         Field("enabled", &AudioSourceComponent::enabled),
         Field("playOnStart", &AudioSourceComponent::playOnStart),
         Field("volume", &AudioSourceComponent::volume),
         Field("loop", &AudioSourceComponent::loop)},
        [](const void* object)
        { return ValidateAudioSource(*static_cast<const AudioSourceComponent*>(object)); }});
}
} // namespace Janus
