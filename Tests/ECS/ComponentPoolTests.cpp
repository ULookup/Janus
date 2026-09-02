#include "ECS/ComponentPool.h"
#include "ECS/Entity.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
struct Position
{
    int x = 0;
    int y = 0;
};
}

TEST_CASE("ComponentPool stores and replaces components", "[ecs][pool]")
{
    Janus::ECS::ComponentPool<Position> pool;
    const Janus::ECS::Entity entity{0, 1};

    pool.Add(entity, Position{3, 4});

    REQUIRE(pool.Has(entity));
    REQUIRE(pool.Get(entity) != nullptr);
    REQUIRE(pool.Get(entity)->x == 3);
    REQUIRE(pool.Get(entity)->y == 4);

    pool.Add(entity, Position{9, 10});
    REQUIRE(pool.Get(entity)->x == 9);
    REQUIRE(pool.Entities().size() == 1);

    pool.Remove(entity);
    REQUIRE_FALSE(pool.Has(entity));
    REQUIRE(pool.Get(entity) == nullptr);
    REQUIRE(pool.Entities().empty());
}
