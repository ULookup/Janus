#include "ECS/Registry.h"
#include "ECS/View.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
struct A
{
    int value = 0;
};

struct B
{
    int value = 0;
};
}

TEST_CASE("View iterates entities with all requested components", "[ecs][view]")
{
    Janus::ECS::Registry registry;

    const auto onlyA = registry.CreateEntity();
    const auto both = registry.CreateEntity();
    const auto onlyB = registry.CreateEntity();

    registry.AddComponent<A>(onlyA, A{1});
    registry.AddComponent<A>(both, A{2});
    registry.AddComponent<B>(both, B{3});
    registry.AddComponent<B>(onlyB, B{4});

    int count = 0;
    registry.View<A, B>().ForEach(
        [&](Janus::ECS::Entity entity, A& a, B& b)
        {
            REQUIRE(entity == both);
            REQUIRE(a.value == 2);
            REQUIRE(b.value == 3);
            ++count;
        });

    REQUIRE(count == 1);
}
