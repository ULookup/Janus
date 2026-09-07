#pragma once
#include "Core/Error/Result.h"
#include "Core/Reflection/ReflectionTypes.h"

namespace Janus
{
struct AnimatorComponent
{
    AssetReferenceValue clip;
    bool enabled = false;
    bool playOnStart = true;
    f32 speed = 1;
};
[[nodiscard]] Result<void> ValidateAnimator(const AnimatorComponent& component);
class ReflectionRegistry;
[[nodiscard]] Result<void> RegisterAnimationReflection(ReflectionRegistry& registry);
} // namespace Janus
