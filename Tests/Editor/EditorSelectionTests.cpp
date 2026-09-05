#include "EditorContext.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "EditorSelection resolves UUID against current Scene handles",
    "[editor][selection][v0.6]")
{
    Janus::Scene scene;
    const auto entity = scene.CreateEntity("Selected");

    const auto* identity =
        scene.GetComponent<Janus::EntityIdentityComponent>(entity);
    REQUIRE(identity != nullptr);

    Janus::Editor::EditorSelection selection;
    selection.Select(identity->id);

    REQUIRE(selection.HasSelection());
    REQUIRE(selection.Resolve(scene) == entity);
    REQUIRE(selection.Validate(scene));

    REQUIRE(scene.DestroyEntity(entity));

    REQUIRE_FALSE(selection.Validate(scene));
    REQUIRE_FALSE(selection.HasSelection());
    REQUIRE_FALSE(selection.Resolve(scene).IsValid());
}

TEST_CASE(
    "EditorSelection clears invalid UUID input",
    "[editor][selection][v0.6]")
{
    Janus::Editor::EditorSelection selection;
    selection.Select(Janus::UUID::Random());
    REQUIRE(selection.HasSelection());

    selection.Select(Janus::UUID{});
    REQUIRE_FALSE(selection.HasSelection());
}
