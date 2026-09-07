#pragma once
#include "Core/Error/Result.h"
#include "Core/Math/Vector2.h"
#include <string>

namespace Janus
{
class ReflectionRegistry;
struct RigidBody2DComponent
{
    std::string type = "static";
    bool enabled = false;
    f32 gravityScale = 1;
    bool fixedRotation = false;
    bool operator==(const RigidBody2DComponent&) const = default;
};
struct Collider2DComponent
{
    Vector2 halfSize{0.5f, 0.5f};
    f32 density = 1;
    f32 friction = 0.3f;
    f32 restitution = 0;
    bool isTrigger = false;
    bool operator==(const Collider2DComponent& other) const
    {
        return halfSize.x == other.halfSize.x && halfSize.y == other.halfSize.y &&
               density == other.density && friction == other.friction &&
               restitution == other.restitution && isTrigger == other.isTrigger;
    }
};
[[nodiscard]] Result<void> ValidateRigidBody2D(const RigidBody2DComponent& value);
[[nodiscard]] Result<void> ValidateCollider2D(const Collider2DComponent& value);
[[nodiscard]] Result<void> RegisterPhysicsReflection(ReflectionRegistry& registry);
} // namespace Janus
