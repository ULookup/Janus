#include "ECS/Entity.h"
#include "ECS/SparseSet.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SparseSet adds and removes entities with swap-and-pop", "[ecs][sparse]")
{
    Janus::ECS::SparseSet set;
    const Janus::ECS::Entity a{0, 1};
    const Janus::ECS::Entity b{1, 1};
    const Janus::ECS::Entity c{2, 1};

    set.Add(a);
    set.Add(b);
    set.Add(c);

    REQUIRE(set.Contains(a));
    REQUIRE(set.Contains(b));
    REQUIRE(set.Contains(c));
    REQUIRE(set.Entities().size() == 3);

    set.Remove(b);

    REQUIRE_FALSE(set.Contains(b));
    REQUIRE(set.Contains(a));
    REQUIRE(set.Contains(c));
    REQUIRE(set.Entities().size() == 2);

    REQUIRE(set.DenseIndex(c) < set.Entities().size());
}

TEST_CASE("SparseSet rejects stale generation", "[ecs][sparse]")
{
    Janus::ECS::SparseSet set;
    set.Add(Janus::ECS::Entity{0, 1});

    REQUIRE_FALSE(set.Contains(Janus::ECS::Entity{0, 2}));
}
