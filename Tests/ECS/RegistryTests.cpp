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
