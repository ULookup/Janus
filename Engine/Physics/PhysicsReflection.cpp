#include "Core/Reflection/ReflectionRegistry.h"
#include "Physics/PhysicsComponents.h"
#include <cmath>

namespace Janus
{
Result<void> ValidateRigidBody2D(const RigidBody2DComponent& value)
{
    if ((value.type != "static" && value.type != "kinematic" && value.type != "dynamic") ||
        !std::isfinite(value.gravityScale) || std::abs(value.gravityScale) > 10)
        return Result<void>::Failure(ErrorCode::InvalidArgument,
                                     "Invalid RigidBody2D type or gravityScale ([-10,10]).");
    return Result<void>::Success();
}
Result<void> ValidateCollider2D(const Collider2DComponent& value)
{
    auto bounded = [](f32 v, f32 low, f32 high)
    { return std::isfinite(v) && v >= low && v <= high; };
    if (!bounded(value.halfSize.x, 0.01f, 100) || !bounded(value.halfSize.y, 0.01f, 100) ||
        !bounded(value.density, 0.01f, 100) || !bounded(value.friction, 0, 1) ||
        !bounded(value.restitution, 0, 1))
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Invalid Collider2D size [0.01,100], density [0.01,100], or material [0,1].");
    return Result<void>::Success();
}
namespace
{
template <typename C, typename T>
PropertyDescriptor Field(const char* component, const char* name, T C::* member,
                         Result<void> (*validate)(const C&))
{
    PropertyDescriptor property;
    property.id = MakePropertyId(std::string(component) + "." + name);
    property.name = property.serializedName = name;
    property.type = GetPropertyType(PropertyValue{T{}});
    property.getter = [member](const void* object)
    { return Result<PropertyValue>::Success(static_cast<const C*>(object)->*member); };
    property.setter = [member, validate](void* object, const PropertyValue& value)
    {
        auto next = *static_cast<C*>(object);
        next.*member = std::get<T>(value);
        auto valid = validate(next);
        if (!valid)
            return valid;
        *static_cast<C*>(object) = std::move(next);
        return Result<void>::Success();
    };
    return property;
}
} // namespace
Result<void> RegisterPhysicsReflection(ReflectionRegistry& registry)
{
    auto body = registry.RegisterComponent(ComponentDescriptor{
        MakeComponentTypeId("RigidBody2D"),
        "RigidBody2D",
        "RigidBody2D",
        true,
        true,
        {Field("RigidBody2D", "type", &RigidBody2DComponent::type, ValidateRigidBody2D),
         Field("RigidBody2D", "enabled", &RigidBody2DComponent::enabled, ValidateRigidBody2D),
         Field("RigidBody2D", "gravityScale", &RigidBody2DComponent::gravityScale,
               ValidateRigidBody2D),
         Field("RigidBody2D", "fixedRotation", &RigidBody2DComponent::fixedRotation,
               ValidateRigidBody2D)},
        [](const void* value)
        { return ValidateRigidBody2D(*static_cast<const RigidBody2DComponent*>(value)); }});
    if (!body)
        return body;
    return registry.RegisterComponent(ComponentDescriptor{
        MakeComponentTypeId("Collider2D"),
        "Collider2D",
        "Collider2D",
        true,
        true,
        {Field("Collider2D", "halfSize", &Collider2DComponent::halfSize, ValidateCollider2D),
         Field("Collider2D", "density", &Collider2DComponent::density, ValidateCollider2D),
         Field("Collider2D", "friction", &Collider2DComponent::friction, ValidateCollider2D),
         Field("Collider2D", "restitution", &Collider2DComponent::restitution, ValidateCollider2D),
         Field("Collider2D", "isTrigger", &Collider2DComponent::isTrigger, ValidateCollider2D)},
        [](const void* value)
        { return ValidateCollider2D(*static_cast<const Collider2DComponent*>(value)); }});
}
} // namespace Janus
