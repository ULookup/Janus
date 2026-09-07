#pragma once
#include "Core/Error/Result.h"
namespace Janus
{
class ReflectionRegistry;
[[nodiscard]] Result<void> RegisterUIReflection(ReflectionRegistry& registry);
} // namespace Janus
