#include "Physics/PhysicsSystem.h"
#include "Scene/Scene.h"
#include <algorithm>
#include <box2d/box2d.h>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace Janus
{
namespace
{
bool Bounded(Vector2 value, f32 limit)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::abs(value.x) <= limit &&
           std::abs(value.y) <= limit;
}
Result<void> Invalid(const char* message)
{
    return Result<void>::Failure(ErrorCode::InvalidArgument, message);
}
} // namespace
struct PhysicsSystem::Impl
{
    struct Body
    {
        b2BodyId native;
        b2ShapeId shape;
        RigidBody2DComponent config;
        Collider2DComponent collider;
        Vector2 position;
        f32 rotation;
    };
    explicit Impl(Scene& value) : scene(value) {}
    Scene& scene;
    b2WorldId world = b2_nullWorldId;
    std::map<UUID, Body> bodies;
    std::map<u64, UUID> shapes;
    std::set<UUID> pending;
    f64 accumulator = 0;
    f64 dropped = 0;
    u64 ticks = 0;

    void RemoveMissing()
    {
        for (auto it = bodies.begin(); it != bodies.end();)
        {
            if (scene.FindEntity(it->first).IsValid())
            {
                ++it;
                continue;
            }
            shapes.erase(b2StoreShapeId(it->second.shape));
            b2DestroyBody(it->second.native);
            it = bodies.erase(it);
        }
    }
    Result<void> FlushDestroyed(const DestructionSink& destroying)
    {
        if (pending.empty())
            return Result<void>::Success();
        // Snapshot the batch: OnDestroy may queue more work for the next safe boundary.
        const auto batch = pending;
        std::set<UUID> descendants;
        for (auto entity : scene.GetEntities())
        {
            auto ancestor = entity;
            while (ancestor.IsValid())
            {
                const auto id = scene.GetComponent<EntityIdentityComponent>(ancestor)->id;
                if (batch.contains(id))
                {
                    descendants.insert(scene.GetComponent<EntityIdentityComponent>(entity)->id);
                    break;
                }
                const auto* hierarchy = scene.GetComponent<HierarchyComponent>(ancestor);
                ancestor = hierarchy ? hierarchy->parent : ECS::Entity{};
            }
        }
        std::optional<Error> firstError;
        if (destroying)
            for (auto id : descendants)
            {
                auto result = destroying(id);
                if (!result && !firstError)
                    firstError = result.GetError();
            }
        for (auto id : batch)
        {
            const auto entity = scene.FindEntity(id);
            if (entity.IsValid())
                scene.DestroyEntity(entity);
            pending.erase(id);
        }
        for (auto id : descendants)
            pending.erase(id);
        RemoveMissing();
        return firstError ? Result<void>::Failure(*firstError) : Result<void>::Success();
    }
    Result<void> Synchronize()
    {
        RemoveMissing();
        usize count = 0;
        for (const auto entity : scene.GetEntities())
        {
            const auto id = scene.GetComponent<EntityIdentityComponent>(entity)->id;
            const auto* body = scene.GetComponent<RigidBody2DComponent>(entity);
            const auto* collider = scene.GetComponent<Collider2DComponent>(entity);
            auto existing = bodies.find(id);
            if (existing != bodies.end() &&
                (!body || !collider || *body != existing->second.config ||
                 *collider != existing->second.collider))
                return Invalid("Physics component settings cannot change during execution; restart "
                               "the Runtime.");
            if (!body || !body->enabled)
                continue;
            if (++count > MaxBodies)
                return Invalid("Physics exceeds the 1024 body limit.");
            auto valid = ValidateRigidBody2D(*body);
            if (!valid)
                return valid;
            if (!collider)
                return Invalid("Enabled RigidBody2D requires Collider2D.");
            valid = ValidateCollider2D(*collider);
            if (!valid)
                return valid;
            const auto* transform = scene.GetComponent<TransformComponent>(entity);
            const auto* hierarchy = scene.GetComponent<HierarchyComponent>(entity);
            if (!transform || (hierarchy && hierarchy->parent.IsValid()) ||
                transform->scale.x != 1 || transform->scale.y != 1 ||
                !Bounded(transform->position, 10000) ||
                !std::isfinite(transform->rotationRadians) ||
                std::abs(transform->rotationRadians) > 10000)
                return Invalid(
                    "Physics needs a root Transform with unit scale, finite pose within +/-10000.");
        }
        // Validate the whole scene before creating bodies or applying teleports.
        std::map<UUID, ECS::Entity> ordered;
        for (const auto entity : scene.GetEntities())
        {
            const auto* body = scene.GetComponent<RigidBody2DComponent>(entity);
            if (body && body->enabled)
                ordered.emplace(scene.GetComponent<EntityIdentityComponent>(entity)->id, entity);
        }
        for (const auto& [id, entity] : ordered)
        {
            const auto& config = *scene.GetComponent<RigidBody2DComponent>(entity);
            const auto& collider = *scene.GetComponent<Collider2DComponent>(entity);
            const auto& transform = *scene.GetComponent<TransformComponent>(entity);
            auto found = bodies.find(id);
            if (found == bodies.end())
            {
                auto definition = b2DefaultBodyDef();
                definition.type =
                    config.type == "dynamic"
                        ? b2_dynamicBody
                        : (config.type == "kinematic" ? b2_kinematicBody : b2_staticBody);
                definition.position = {transform.position.x, transform.position.y};
                definition.rotation = b2MakeRot(transform.rotationRadians);
                definition.gravityScale = config.gravityScale;
                definition.fixedRotation = config.fixedRotation;
                auto native = b2CreateBody(world, &definition);
                auto shapeDef = b2DefaultShapeDef();
                shapeDef.density = collider.density;
                shapeDef.material.friction = collider.friction;
                shapeDef.material.restitution = collider.restitution;
                shapeDef.isSensor = collider.isTrigger;
                shapeDef.enableSensorEvents = true;
                shapeDef.enableContactEvents = true;
                auto polygon = b2MakeBox(collider.halfSize.x, collider.halfSize.y);
                auto shape = b2CreatePolygonShape(native, &shapeDef, &polygon);
                shapes.emplace(b2StoreShapeId(shape), id);
                bodies.emplace(id, Body{native, shape, config, collider, transform.position,
                                        transform.rotationRadians});
            }
            else
            {
                auto& body = found->second;
                if (body.position.x != transform.position.x ||
                    body.position.y != transform.position.y ||
                    body.rotation != transform.rotationRadians)
                {
                    b2Body_SetTransform(body.native, {transform.position.x, transform.position.y},
                                        b2MakeRot(transform.rotationRadians));
                    b2Body_SetAwake(body.native, true);
                    body.position = transform.position;
                    body.rotation = transform.rotationRadians;
                }
            }
        }
        return Result<void>::Success();
    }
    Result<std::vector<PhysicsEvent>> Events()
    {
        std::vector<PhysicsEvent> events;
        const auto contacts = b2World_GetContactEvents(world);
        const auto sensors = b2World_GetSensorEvents(world);
        const auto total = static_cast<usize>(contacts.beginCount) + contacts.endCount +
                           sensors.beginCount + sensors.endCount;
        if (total > MaxEvents)
            return Result<std::vector<PhysicsEvent>>::Failure(
                ErrorCode::InvalidState, "Physics event batch exceeds 4096 events.");
        auto append = [&](b2ShapeId a, b2ShapeId b, PhysicsEventKind kind)
        {
            auto first = shapes.find(b2StoreShapeId(a));
            auto second = shapes.find(b2StoreShapeId(b));
            // Destruction events can reference invalid backend ids; never dereference them.
            if (first == shapes.end() || second == shapes.end())
                return;
            events.push_back({std::min(first->second, second->second),
                              std::max(first->second, second->second), kind});
        };
        for (int i = 0; i < contacts.beginCount; ++i)
            append(contacts.beginEvents[i].shapeIdA, contacts.beginEvents[i].shapeIdB,
                   PhysicsEventKind::CollisionEnter);
        for (int i = 0; i < contacts.endCount; ++i)
            append(contacts.endEvents[i].shapeIdA, contacts.endEvents[i].shapeIdB,
                   PhysicsEventKind::CollisionExit);
        for (int i = 0; i < sensors.beginCount; ++i)
            append(sensors.beginEvents[i].sensorShapeId, sensors.beginEvents[i].visitorShapeId,
                   PhysicsEventKind::TriggerEnter);
        for (int i = 0; i < sensors.endCount; ++i)
            append(sensors.endEvents[i].sensorShapeId, sensors.endEvents[i].visitorShapeId,
                   PhysicsEventKind::TriggerExit);
        auto key = [](const PhysicsEvent& e) { return std::tuple{e.first, e.second, e.kind}; };
        std::sort(events.begin(), events.end(),
                  [&](const auto& a, const auto& b) { return key(a) < key(b); });
        events.erase(std::unique(events.begin(), events.end(),
                                 [&](const auto& a, const auto& b) { return key(a) == key(b); }),
                     events.end());
        return Result<std::vector<PhysicsEvent>>::Success(std::move(events));
    }
};

PhysicsSystem::PhysicsSystem(Scene& scene) : m_Impl(std::make_unique<Impl>(scene)) {}
PhysicsSystem::~PhysicsSystem()
{
    Stop();
}
Result<void> PhysicsSystem::Start()
{
    if (B2_IS_NON_NULL(m_Impl->world))
        return Invalid("Physics is already running.");
    auto definition = b2DefaultWorldDef();
    definition.gravity = {0, -9.81f};
    m_Impl->world = b2CreateWorld(&definition);
    auto result = m_Impl->Synchronize();
    if (!result)
        Stop();
    return result;
}
void PhysicsSystem::Stop() noexcept
{
    if (B2_IS_NON_NULL(m_Impl->world))
        b2DestroyWorld(m_Impl->world);
    m_Impl->world = b2_nullWorldId;
    m_Impl->bodies.clear();
    m_Impl->shapes.clear();
    m_Impl->pending.clear();
    m_Impl->accumulator = m_Impl->dropped = 0;
    m_Impl->ticks = 0;
}
Result<void> PhysicsSystem::Advance(TimeStep step, const EventSink& sink,
                                    const DestructionSink& destroying)
{
    auto& impl = *m_Impl;
    if (B2_IS_NULL(impl.world))
        return Invalid("Physics must be started before Advance.");
    if (!std::isfinite(step.GetSeconds()))
        return Invalid("Invalid physics timestep.");
    auto flushed = impl.FlushDestroyed(destroying);
    if (!flushed)
        return flushed;
    auto synchronized = impl.Synchronize();
    if (!synchronized)
        return synchronized;
    // Reduce huge finite durations before addition, avoiding overflow and unbounded loops.
    const auto seconds = step.GetSeconds();
    const auto budget = FixedSeconds * static_cast<f64>(MaxCatchUpTicks);
    const auto accepted =
        seconds > budget ? budget + std::fmod(seconds - budget, FixedSeconds) : seconds;
    impl.dropped = std::min(1e12, impl.dropped + (seconds - accepted));
    impl.accumulator += accepted;
    usize steps = 0;
    while (impl.accumulator + 1e-12 >= FixedSeconds && steps < MaxCatchUpTicks)
    {
        impl.accumulator = std::max(0.0, impl.accumulator - FixedSeconds);
        ++steps;
        b2World_Step(impl.world, static_cast<f32>(FixedSeconds), 4);
        ++impl.ticks;
        for (auto& [id, body] : impl.bodies)
        {
            const auto position = b2Body_GetPosition(body.native);
            body.position = {position.x, position.y};
            body.rotation = b2Rot_GetAngle(b2Body_GetRotation(body.native));
            if (!Bounded(body.position, 10000))
                return Invalid("Physics body left the supported world bounds.");
            auto* transform =
                impl.scene.GetComponent<TransformComponent>(impl.scene.FindEntity(id));
            transform->position = body.position;
            transform->rotationRadians = body.rotation;
            transform->dirty = true;
        }
        auto events = impl.Events();
        if (!events)
            return Result<void>::Failure(events.GetError());
        for (const auto& event : events.Value())
        {
            if (!sink || IsPendingDestroy(event.first) || IsPendingDestroy(event.second))
                continue;
            auto delivered = sink(event);
            if (!delivered)
            {
                static_cast<void>(impl.FlushDestroyed(destroying));
                return delivered;
            }
        }
        flushed = impl.FlushDestroyed(destroying);
        if (!flushed)
            return flushed;
        synchronized = impl.Synchronize();
        if (!synchronized)
            return synchronized;
    }
    if (impl.accumulator + 1e-12 >= FixedSeconds)
    {
        impl.accumulator = std::max(0.0, impl.accumulator - FixedSeconds);
        impl.dropped = std::min(1e12, impl.dropped + FixedSeconds);
    }
    return Result<void>::Success();
}
Result<void> PhysicsSystem::SetVelocity(UUID id, Vector2 velocity)
{
    const auto found = m_Impl->bodies.find(id);
    if (!Bounded(velocity, 100) || found == m_Impl->bodies.end() ||
        !m_Impl->scene.FindEntity(id).IsValid() || IsPendingDestroy(id) ||
        found->second.config.type == "static")
        return Invalid(
            "Velocity needs a live movable body and finite components within +/-100 m/s.");
    b2Body_SetLinearVelocity(found->second.native, {velocity.x, velocity.y});
    b2Body_SetAwake(found->second.native, true);
    return Result<void>::Success();
}
Result<Vector2> PhysicsSystem::GetVelocity(UUID id) const
{
    const auto found = m_Impl->bodies.find(id);
    if (found == m_Impl->bodies.end() || !m_Impl->scene.FindEntity(id).IsValid() ||
        IsPendingDestroy(id))
        return Result<Vector2>::Failure(ErrorCode::InvalidState,
                                        "Entity has no live physics body.");
    const auto velocity = b2Body_GetLinearVelocity(found->second.native);
    return Result<Vector2>::Success({velocity.x, velocity.y});
}
Result<void> PhysicsSystem::ApplyImpulse(UUID id, Vector2 impulse)
{
    const auto found = m_Impl->bodies.find(id);
    if (!Bounded(impulse, 100) || found == m_Impl->bodies.end() ||
        !m_Impl->scene.FindEntity(id).IsValid() || IsPendingDestroy(id) ||
        found->second.config.type != "dynamic")
        return Invalid(
            "Impulse needs a live dynamic body and finite components within +/-100 N.s.");
    b2Body_ApplyLinearImpulseToCenter(found->second.native, {impulse.x, impulse.y}, true);
    return Result<void>::Success();
}
Result<void> PhysicsSystem::SetPosition(UUID id, Vector2 position)
{
    const auto entity = m_Impl->scene.FindEntity(id);
    auto* transform = m_Impl->scene.GetComponent<TransformComponent>(entity);
    if (B2_IS_NULL(m_Impl->world) || !transform || IsPendingDestroy(id) ||
        !Bounded(position, 10000))
        return Invalid("Teleport needs a live entity and finite position within +/-10000 metres.");
    transform->position = position;
    transform->dirty = true;
    auto found = m_Impl->bodies.find(id);
    if (found != m_Impl->bodies.end())
    {
        b2Body_SetTransform(found->second.native, {position.x, position.y},
                            b2MakeRot(found->second.rotation));
        b2Body_SetAwake(found->second.native, true);
        found->second.position = position;
    }
    return Result<void>::Success();
}
Result<void> PhysicsSystem::QueueDestroy(UUID id)
{
    if (B2_IS_NULL(m_Impl->world) || !m_Impl->scene.FindEntity(id).IsValid())
        return Invalid("Destroy needs a live runtime entity.");
    if (m_Impl->pending.size() >= MaxBodies && !m_Impl->pending.contains(id))
        return Invalid("Physics deferred destruction limit reached.");
    m_Impl->pending.insert(id);
    return Result<void>::Success();
}
bool PhysicsSystem::IsPendingDestroy(UUID id) const
{
    auto entity = m_Impl->scene.FindEntity(id);
    while (entity.IsValid())
    {
        const auto* identity = m_Impl->scene.GetComponent<EntityIdentityComponent>(entity);
        if (identity && m_Impl->pending.contains(identity->id))
            return true;
        const auto* hierarchy = m_Impl->scene.GetComponent<HierarchyComponent>(entity);
        entity = hierarchy ? hierarchy->parent : ECS::Entity{};
    }
    return false;
}
Result<std::optional<PhysicsRayHit>> PhysicsSystem::Raycast(Vector2 start, Vector2 end,
                                                            bool includeTriggers) const
{
    using Return = Result<std::optional<PhysicsRayHit>>;
    if (B2_IS_NULL(m_Impl->world) || !Bounded(start, 10000) || !Bounded(end, 10000) ||
        (start.x == end.x && start.y == end.y))
        return Return::Failure(
            ErrorCode::InvalidArgument,
            "Raycast needs a running world and distinct finite endpoints within +/-10000.");
    struct Context
    {
        const PhysicsSystem& system;
        bool triggers;
        std::optional<PhysicsRayHit> hit;
    } context{*this, includeTriggers, {}};
    b2World_CastRay(
        m_Impl->world, {start.x, start.y}, {end.x - start.x, end.y - start.y},
        b2DefaultQueryFilter(),
        [](b2ShapeId shape, b2Vec2 point, b2Vec2 normal, float fraction, void* opaque) -> float
        {
            auto& ctx = *static_cast<Context*>(opaque);
            auto found = ctx.system.m_Impl->shapes.find(b2StoreShapeId(shape));
            if (found == ctx.system.m_Impl->shapes.end() ||
                (!ctx.triggers && b2Shape_IsSensor(shape)) ||
                ctx.system.IsPendingDestroy(found->second) ||
                !ctx.system.m_Impl->scene.FindEntity(found->second).IsValid())
                return -1;
            if (!ctx.hit || fraction < ctx.hit->fraction ||
                (fraction == ctx.hit->fraction && found->second < ctx.hit->entity))
                ctx.hit = PhysicsRayHit{
                    found->second, {point.x, point.y}, {normal.x, normal.y}, fraction};
            return 1; // Visit all hits so UUID breaks equal-distance ties consistently.
        },
        &context);
    return Return::Success(context.hit);
}
u64 PhysicsSystem::GetTickCount() const noexcept
{
    return m_Impl->ticks;
}
f64 PhysicsSystem::GetDroppedSeconds() const noexcept
{
    return m_Impl->dropped;
}
usize PhysicsSystem::GetBodyCount() const noexcept
{
    return m_Impl->bodies.size();
}
} // namespace Janus
