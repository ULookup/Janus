#include "Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Scene links parent and child hierarchy", "[scene][hierarchy]")
{
    Janus::Scene scene;
    const auto parent = scene.CreateEntity();
    const auto child = scene.CreateEntity();

    REQUIRE(scene.SetParent(child, parent));

    const auto* parentHierarchy =
        scene.GetComponent<Janus::HierarchyComponent>(parent);
    const auto* childHierarchy =
        scene.GetComponent<Janus::HierarchyComponent>(child);

    REQUIRE(parentHierarchy->firstChild == child);
    REQUIRE(childHierarchy->parent == parent);
    REQUIRE(childHierarchy->previousSibling == Janus::ECS::Entity{});
}

TEST_CASE("Scene rejects hierarchy cycles", "[scene][hierarchy]")
{
    Janus::Scene scene;
    const auto a = scene.CreateEntity();
    const auto b = scene.CreateEntity();

    REQUIRE(scene.SetParent(b, a));

    const auto cycle = scene.SetParent(a, b);
    REQUIRE_FALSE(cycle);
    REQUIRE(cycle.GetError().code == Janus::ErrorCode::HierarchyCycle);
}
