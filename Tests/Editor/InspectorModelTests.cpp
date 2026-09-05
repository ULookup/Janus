#include "InspectorModel.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>
#include <utility>

namespace
{

const Janus::Editor::InspectorComponentModel* FindComponent(
    const std::vector<Janus::Editor::InspectorComponentModel>& model,
    std::string_view name)
{
    const auto found =
        std::find_if(
            model.begin(),
            model.end(),
            [name](const Janus::Editor::InspectorComponentModel& component)
            {
                return component.descriptor != nullptr
                    && component.descriptor->name == name;
            });

    return found == model.end()
        ? nullptr
        : &*found;
}

bool HasProperty(
    const Janus::Editor::InspectorComponentModel& component,
    std::string_view name)
{
    return std::any_of(
        component.properties.begin(),
        component.properties.end(),
        [name](const Janus::Editor::InspectorPropertyModel& property)
        {
            return property.descriptor != nullptr
                && property.descriptor->name == name;
        });
}

} // namespace

TEST_CASE(
    "Inspector model enumerates reflected component presence and visible properties",
    "[editor][inspector][reflection][v0.7]")
{
    auto registryResult =
        Janus::CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registryResult);

    auto registry =
        std::move(registryResult).Value();

    Janus::Scene scene;
    const auto entity =
        scene.CreateEntity("Inspectable");

    const auto* identity =
        scene.GetComponent<Janus::EntityIdentityComponent>(
            entity);
    REQUIRE(identity != nullptr);

    scene.AddComponent<Janus::SpriteRendererComponent>(
        entity,
        Janus::SpriteRendererComponent{});

    const auto modelResult =
        Janus::Editor::BuildInspectorModel(
            scene,
            identity->id,
            registry);
    REQUIRE(modelResult);

    const auto& model =
        modelResult.Value();

    REQUIRE(model.size() == 4);

    const auto* transform =
        FindComponent(
            model,
            "Transform");
    REQUIRE(transform != nullptr);
    REQUIRE(transform->present);
    REQUIRE(transform->properties.size() == 3);
    REQUIRE(HasProperty(*transform, "position"));
    REQUIRE(HasProperty(*transform, "rotation"));
    REQUIRE(HasProperty(*transform, "scale"));

    const auto* sprite =
        FindComponent(
            model,
            "SpriteRenderer");
    REQUIRE(sprite != nullptr);
    REQUIRE(sprite->present);
    REQUIRE(HasProperty(*sprite, "texture"));
    REQUIRE(HasProperty(*sprite, "size"));
    REQUIRE(HasProperty(*sprite, "color"));
    REQUIRE(HasProperty(*sprite, "layer"));
    REQUIRE(HasProperty(*sprite, "enabled"));
    REQUIRE_FALSE(HasProperty(*sprite, "uvMin"));
    REQUIRE_FALSE(HasProperty(*sprite, "uvMax"));

    const auto* camera =
        FindComponent(
            model,
            "Camera");
    REQUIRE(camera != nullptr);
    REQUIRE_FALSE(camera->present);
    REQUIRE(camera->properties.empty());
    REQUIRE(camera->descriptor->removable);

    const auto* script =
        FindComponent(
            model,
            "LuaScript");
    REQUIRE(script != nullptr);
    REQUIRE_FALSE(script->present);
    REQUIRE(script->properties.empty());
    REQUIRE(script->descriptor->removable);
}

TEST_CASE(
    "Inspector model rejects invalid entity identity",
    "[editor][inspector][reflection][errors][v0.7]")
{
    auto registryResult =
        Janus::CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registryResult);

    Janus::Scene scene;

    const auto invalid =
        Janus::Editor::BuildInspectorModel(
            scene,
            Janus::UUID{},
            registryResult.Value());
    REQUIRE_FALSE(invalid);
    REQUIRE(
        invalid.GetError().code
        == Janus::ErrorCode::InvalidArgument);

    const auto missing =
        Janus::Editor::BuildInspectorModel(
            scene,
            Janus::UUID::Random(),
            registryResult.Value());
    REQUIRE_FALSE(missing);
    REQUIRE(
        missing.GetError().code
        == Janus::ErrorCode::EntityNotFound);
}
