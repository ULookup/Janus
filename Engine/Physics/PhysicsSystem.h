#pragma once
#include "Core/Time/TimeStep.h"
#include "Core/UUID/UUID.h"
#include "Physics/PhysicsComponents.h"
#include <functional>
#include <memory>
#include <optional>

namespace Janus
{
class Scene;
enum class PhysicsEventKind
{
    CollisionEnter,
    CollisionExit,
    TriggerEnter,
    TriggerExit
};
struct PhysicsEvent
{
    UUID first;
    UUID second;
    PhysicsEventKind kind;
};
struct PhysicsRayHit
{
    UUID entity;
    Vector2 point;
    Vector2 normal;
    f32 fraction = 0;
};

// Owner-thread world. Scene must outlive it; backend ids never cross this API.
class PhysicsSystem final
{
  public:
    using DestructionSink = std::function<Result<void>(UUID)>;
    using EventSink = std::function<Result<void>(const PhysicsEvent&)>;
    static constexpr f64 FixedSeconds = 1.0 / 60.0;
    static constexpr usize MaxBodies = 1024;
    static constexpr usize MaxEvents = 4096;
    static constexpr usize MaxCatchUpTicks = 8;
    explicit PhysicsSystem(Scene& scene);
    ~PhysicsSystem();
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    [[nodiscard]] Result<void> Start();
    void Stop() noexcept;
    [[nodiscard]] Result<void> Advance(TimeStep step, const EventSink& sink = {},
                                       const DestructionSink& destroying = {});
    [[nodiscard]] Result<void> SetVelocity(UUID entity, Vector2 velocity);
    [[nodiscard]] Result<Vector2> GetVelocity(UUID entity) const;
    [[nodiscard]] Result<void> ApplyImpulse(UUID entity, Vector2 impulse);
    [[nodiscard]] Result<void> SetPosition(UUID entity, Vector2 position);
    [[nodiscard]] Result<void> QueueDestroy(UUID entity);
    [[nodiscard]] bool IsPendingDestroy(UUID entity) const;
    [[nodiscard]] Result<std::optional<PhysicsRayHit>> Raycast(Vector2 start, Vector2 end,
                                                               bool includeTriggers = false) const;
    [[nodiscard]] u64 GetTickCount() const noexcept;
    [[nodiscard]] f64 GetDroppedSeconds() const noexcept;
    [[nodiscard]] usize GetBodyCount() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
} // namespace Janus
