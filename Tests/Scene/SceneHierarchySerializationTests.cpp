#include "Scene/SceneDeserializer.h"
#include "Scene/SceneSerializer.h"
#include "Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>

TEST_CASE("Scene serialization preserves sibling order deterministically",
          "[scene][serialization][hierarchy]")
{
    Janus::Scene scene;
    const auto parent = scene.CreateEntity("Parent");
    const auto first = scene.CreateEntity("First");
    const auto second = scene.CreateEntity("Second");
    const auto third = scene.CreateEntity("Third");

    REQUIRE(scene.SetParent(first, parent));
    REQUIRE(scene.SetParent(second, parent));
    REQUIRE(scene.SetParent(third, parent));

    const auto* parentHierarchy =
        scene.GetComponent<Janus::HierarchyComponent>(parent);
    REQUIRE(parentHierarchy->firstChild == third);
    REQUIRE(scene.GetComponent<Janus::HierarchyComponent>(third)->nextSibling
            == second);
    REQUIRE(scene.GetComponent<Janus::HierarchyComponent>(second)->nextSibling
            == first);

    auto serialized = Janus::SceneSerializer::Serialize(scene);
    REQUIRE(serialized);

    auto loadedResult =
        Janus::SceneDeserializer::Deserialize(serialized.Value());
    REQUIRE(loadedResult);
    auto loaded = std::move(loadedResult).Value();

    const Janus::UUID parentId =
        scene.GetComponent<Janus::EntityIdentityComponent>(parent)->id;
    const Janus::UUID firstId =
        scene.GetComponent<Janus::EntityIdentityComponent>(first)->id;
    const Janus::UUID secondId =
        scene.GetComponent<Janus::EntityIdentityComponent>(second)->id;
    const Janus::UUID thirdId =
        scene.GetComponent<Janus::EntityIdentityComponent>(third)->id;

    const auto loadedParent = loaded->FindEntity(parentId);
    const auto loadedFirst = loaded->FindEntity(firstId);
    const auto loadedSecond = loaded->FindEntity(secondId);
    const auto loadedThird = loaded->FindEntity(thirdId);

    const auto* loadedParentHierarchy =
        loaded->GetComponent<Janus::HierarchyComponent>(loadedParent);
    REQUIRE(loadedParentHierarchy->firstChild == loadedThird);
    REQUIRE(loaded->GetComponent<Janus::HierarchyComponent>(loadedThird)->nextSibling
            == loadedSecond);
    REQUIRE(loaded->GetComponent<Janus::HierarchyComponent>(loadedSecond)->nextSibling
            == loadedFirst);

    auto reserialized = Janus::SceneSerializer::Serialize(*loaded);
    REQUIRE(reserialized);
    REQUIRE(reserialized.Value() == serialized.Value());
}
