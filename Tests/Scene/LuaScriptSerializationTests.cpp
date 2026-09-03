#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneSerializer.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace
{

Janus::UUID ParseUUID(const char* text)
{
    return Janus::UUID::Parse(text).Value();
}

Janus::AssetHandle ParseHandle(const char* text)
{
    return Janus::AssetHandle::Parse(text).Value();
}

} // namespace

TEST_CASE("LuaScriptComponent round trips persistent Script AssetHandle",
          "[scene][serialization][lua-script][v0.5]")
{
    const Janus::UUID sceneId =
        ParseUUID("51000000-0000-4000-8000-000000000001");
    const Janus::UUID entityId =
        ParseUUID("52000000-0000-4000-8000-000000000001");
    const Janus::AssetHandle script =
        ParseHandle("53000000-0000-4000-8000-000000000001");

    Janus::Scene scene(Janus::SceneMetadata{sceneId, "ScriptScene"});
    const auto entity =
        scene.CreateEntityWithUUID(entityId, "Player").Value();
    scene.AddComponent<Janus::LuaScriptComponent>(
        entity,
        Janus::LuaScriptComponent{script, true});

    const auto serialized = Janus::SceneSerializer::Serialize(scene);
    REQUIRE(serialized);
    REQUIRE(serialized.Value().find("\"LuaScript\"") != std::string::npos);
    REQUIRE(serialized.Value().find(script.ToString()) != std::string::npos);
    REQUIRE(serialized.Value().find("Scripts/") == std::string::npos);

    auto loadedResult = Janus::SceneDeserializer::Deserialize(
        serialized.Value());
    REQUIRE(loadedResult);
    auto loaded = std::move(loadedResult).Value();

    const auto loadedEntity = loaded->FindEntity(entityId);
    REQUIRE(loadedEntity.IsValid());
    const auto* loadedScript =
        loaded->GetComponent<Janus::LuaScriptComponent>(loadedEntity);
    REQUIRE(loadedScript != nullptr);
    REQUIRE(loadedScript->script == script);
    REQUIRE(loadedScript->enabled);

    const auto second = Janus::SceneSerializer::Serialize(*loaded);
    REQUIRE(second);
    REQUIRE(second.Value() == serialized.Value());
}

TEST_CASE("Disabled LuaScriptComponent may persist without a Script handle",
          "[scene][serialization][lua-script][v0.5]")
{
    Janus::Scene scene(Janus::SceneMetadata{
        ParseUUID("51000000-0000-4000-8000-000000000002"),
        "DisabledScript"});
    const auto entity = scene.CreateEntityWithUUID(
        ParseUUID("52000000-0000-4000-8000-000000000002"),
        "Dormant").Value();
    scene.AddComponent<Janus::LuaScriptComponent>(
        entity,
        Janus::LuaScriptComponent{Janus::AssetHandle{}, false});

    const auto serialized = Janus::SceneSerializer::Serialize(scene);
    REQUIRE(serialized);
    REQUIRE(serialized.Value().find("\"script\": null")
        != std::string::npos);

    const auto loaded = Janus::SceneDeserializer::Deserialize(
        serialized.Value());
    REQUIRE(loaded);
    const auto loadedEntity = loaded.Value()->FindEntity(
        ParseUUID("52000000-0000-4000-8000-000000000002"));
    const auto* script =
        loaded.Value()->GetComponent<Janus::LuaScriptComponent>(loadedEntity);
    REQUIRE(script != nullptr);
    REQUIRE_FALSE(script->script.IsValid());
    REQUIRE_FALSE(script->enabled);
}

TEST_CASE("Enabled LuaScriptComponent rejects missing or malformed handles",
          "[scene][serialization][lua-script][v0.5]")
{
    SECTION("serializer rejects enabled nil handle")
    {
        Janus::Scene scene(Janus::SceneMetadata{
            ParseUUID("51000000-0000-4000-8000-000000000003"),
            "InvalidScript"});
        const auto entity = scene.CreateEntityWithUUID(
            ParseUUID("52000000-0000-4000-8000-000000000003"),
            "Broken").Value();
        scene.AddComponent<Janus::LuaScriptComponent>(
            entity,
            Janus::LuaScriptComponent{Janus::AssetHandle{}, true});

        const auto serialized = Janus::SceneSerializer::Serialize(scene);
        REQUIRE_FALSE(serialized);
        REQUIRE(serialized.GetError().code == Janus::ErrorCode::InvalidState);
    }

    SECTION("deserializer rejects enabled null handle")
    {
        const std::string text = R"json({
          "schema":"janus.scene","version":1,
          "scene":{"id":"51000000-0000-4000-8000-000000000004","name":"Bad"},
          "entities":[{
            "id":"52000000-0000-4000-8000-000000000004",
            "name":"Broken","parent":null,
            "components":{
              "Transform":{"position":[0,0],"rotation":0,"scale":[1,1]},
              "LuaScript":{"script":null,"enabled":true}
            }
          }]
        })json";
        REQUIRE_FALSE(Janus::SceneDeserializer::Deserialize(text));
    }

    SECTION("deserializer rejects malformed handle")
    {
        const std::string text = R"json({
          "schema":"janus.scene","version":1,
          "scene":{"id":"51000000-0000-4000-8000-000000000005","name":"Bad"},
          "entities":[{
            "id":"52000000-0000-4000-8000-000000000005",
            "name":"Broken","parent":null,
            "components":{
              "Transform":{"position":[0,0],"rotation":0,"scale":[1,1]},
              "LuaScript":{"script":"not-a-handle","enabled":true}
            }
          }]
        })json";
        REQUIRE_FALSE(Janus::SceneDeserializer::Deserialize(text));
    }
}
