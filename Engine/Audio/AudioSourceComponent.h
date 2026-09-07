#pragma once
#include "Core/Error/Result.h"
#include "Core/Reflection/ReflectionTypes.h"

namespace Janus
{
struct AudioSourceComponent
{
    AssetReferenceValue clip;
    bool enabled = false;
    bool playOnStart = true;
    f32 volume = 1;
    bool loop = false;
};
[[nodiscard]] Result<void> ValidateAudioSource(const AudioSourceComponent& component);
class ReflectionRegistry;
[[nodiscard]] Result<void> RegisterAudioReflection(ReflectionRegistry& registry);
} // namespace Janus
