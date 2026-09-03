#include "ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

namespace
{
struct Health
{
    int value = 100;
};

struct Name
{
    const char* text = "";
};
}

TEST_CASE("Registry reuses indices and invalidates stale entities", "[ecs][registry]")
{
    Janus::ECS::Registry registry;
    const auto first = registry.CreateEntity();
    const auto second = registry.CreateEntity();

    REQUIRE(registry.IsValid(first));
    REQUIRE(registry.IsValid(second));
    REQUIRE(registry.AliveEntityCount() == 2);

    REQUIRE(registry.DestroyEntity(first));
    REQUIRE_FALSE(registry.IsValid(first));

    const auto reused = registry.CreateEntity();
    REQUIRE(reused.index == first.index);
    REQUIRE(reused.generation != first.generation);
    REQUIRE(registry.IsValid(reused));
}

TEST_CASE("Registry owns component lifetime", "[ecs][registry]")
{
    Janus::ECS::Registry registry;
    const auto entity = registry.CreateEntity();

    registry.AddComponent<Health>(entity, Health{42});
    registry.AddComponent<Name>(entity, Name{"player"});

    REQUIRE(registry.HasComponent<Health>(entity));
    REQUIRE(registry.GetComponent<Health>(entity)->value == 42);
    REQUIRE(
        registry.GetComponent<Name>(entity)->text
        == std::string_view{"player"});

    REQUIRE(registry.RemoveComponent<Health>(entity));
    REQUIRE_FALSE(registry.HasComponent<Health>(entity));

    registry.DestroyEntity(entity);
    REQUIRE_FALSE(registry.HasComponent<Name>(entity));
}

TEST_CASE("Registry rejects adding a component to an invalid entity", "[ecs][registry]")
{
    Janus::ECS::Registry registry;
    const Janus::ECS::Entity invalid;

    REQUIRE(registry.AddComponent<Health>(invalid, Health{1}) == nullptr);
    REQUIRE_FALSE(registry.HasComponent<Health>(invalid));
}

TEST_CASE("Registry rejects component access through stale entity handles", "[ecs][registry]")
{
    Janus::ECS::Registry registry;
    const auto stale = registry.CreateEntity();

    REQUIRE(registry.AddComponent<Health>(stale, Health{42}) != nullptr);
    REQUIRE(registry.DestroyEntity(stale));

    const auto replacement = registry.CreateEntity();
    REQUIRE(replacement.index == stale.index);
    REQUIRE(replacement.generation != stale.generation);
    REQUIRE_FALSE(registry.IsValid(stale));

    REQUIRE(registry.AddComponent<Health>(stale, Health{7}) == nullptr);
    REQUIRE_FALSE(registry.RemoveComponent<Health>(stale));
    REQUIRE(registry.GetComponent<Health>(stale) == nullptr);
    REQUIRE_FALSE(registry.HasComponent<Health>(stale));

    auto* replacementHealth =
        registry.AddComponent<Health>(replacement, Health{99});
    REQUIRE(replacementHealth != nullptr);
    REQUIRE(replacementHealth->value == 99);
    REQUIRE(registry.HasComponent<Health>(replacement));
}
