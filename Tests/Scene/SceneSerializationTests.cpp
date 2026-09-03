#include "Scene/SceneDeserializer.h"
#include "Scene/SceneSerializer.h"
#include "Scene/Scene.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{

Janus::UUID ParseUUID(const char* text)
{
    return Janus::UUID::Parse(text).Value();
}

class SceneTempDirectory final
{
public:
    SceneTempDirectory()
    {
        m_Path = std::filesystem::temp_directory_path()
            / ("janus-scene-"
               + std::to_string(
                   std::chrono::steady_clock::now()
                       .time_since_epoch()
                       .count()));
        std::error_code error;
        if (!std::filesystem::create_directory(m_Path, error) || error)
        {
            throw std::runtime_error("Failed to create Scene test directory.");
        }
    }

    ~SceneTempDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_Path, error);
    }

    const std::filesystem::path& Path() const noexcept
    {
        return m_Path;
    }

private:
    std::filesystem::path m_Path;
};

} // namespace

TEST_CASE("Scene serialization round trips persistent authoring state",
          "[scene][serialization]")
{
    const Janus::UUID sceneId =
        ParseUUID("10000000-0000-4000-8000-000000000001");
    const Janus::UUID rootId =
        ParseUUID("20000000-0000-4000-8000-000000000001");
    const Janus::UUID childId =
        ParseUUID("20000000-0000-4000-8000-000000000002");
    const Janus::UUID grandchildId =
        ParseUUID("20000000-0000-4000-8000-000000000003");
    const Janus::AssetHandle texture = Janus::AssetHandle::Parse(
        "30000000-0000-4000-8000-000000000001").Value();

    Janus::Scene scene(Janus::SceneMetadata{sceneId, "Battle"});
    const auto root = scene.CreateEntityWithUUID(rootId, "Root").Value();
    const auto child = scene.CreateEntityWithUUID(childId, "Player").Value();
    const auto grandchild =
        scene.CreateEntityWithUUID(grandchildId, "Weapon").Value();

    REQUIRE(scene.SetParent(child, root));
    REQUIRE(scene.SetParent(grandchild, child));

    auto* rootTransform = scene.GetComponent<Janus::TransformComponent>(root);
    rootTransform->position = Janus::Vector2{10.0f, 20.0f};
    rootTransform->rotationRadians = 0.25f;
    rootTransform->scale = Janus::Vector2{2.0f, 3.0f};
    rootTransform->worldPosition = Janus::Vector2{999.0f, 999.0f};
    rootTransform->dirty = false;

    scene.AddComponent<Janus::CameraComponent>(
        root,
        Janus::CameraComponent{1.5f, true});

    scene.AddComponent<Janus::SpriteRendererComponent>(
        child,
        Janus::SpriteRendererComponent{
            texture,
            Janus::Vector2{128.0f, 64.0f},
            Janus::Color{0.2f, 0.4f, 0.6f, 0.8f},
            7,
            Janus::TextureRegion{
                Janus::Vector2{0.1f, 0.2f},
                Janus::Vector2{0.8f, 0.9f}},
            false});

    auto serialized = Janus::SceneSerializer::Serialize(scene);
    REQUIRE(serialized);

    const std::string& text = serialized.Value();
    REQUIRE(text.find("\"schema\": \"janus.scene\"") != std::string::npos);
    REQUIRE(text.find("worldPosition") == std::string::npos);
    REQUIRE(text.find("worldScale") == std::string::npos);
    REQUIRE(text.find("dirty") == std::string::npos);
    REQUIRE(text.find("TextureHandle") == std::string::npos);
    REQUIRE(text.find("generation") == std::string::npos);
    REQUIRE(text.find("\"index\"") == std::string::npos);

    auto loadedResult = Janus::SceneDeserializer::Deserialize(text);
    REQUIRE(loadedResult);
    auto loaded = std::move(loadedResult).Value();

    REQUIRE(loaded->GetMetadata().id == sceneId);
    REQUIRE(loaded->GetMetadata().name == "Battle");

    const auto loadedRoot = loaded->FindEntity(rootId);
    const auto loadedChild = loaded->FindEntity(childId);
    const auto loadedGrandchild = loaded->FindEntity(grandchildId);
    REQUIRE(loadedRoot.IsValid());
    REQUIRE(loadedChild.IsValid());
    REQUIRE(loadedGrandchild.IsValid());

    REQUIRE(loaded->GetComponent<Janus::EntityIdentityComponent>(loadedChild)->name
            == "Player");

    const auto* loadedTransform =
        loaded->GetComponent<Janus::TransformComponent>(loadedRoot);
    REQUIRE(loadedTransform->position.x == Catch::Approx(10.0f));
    REQUIRE(loadedTransform->position.y == Catch::Approx(20.0f));
    REQUIRE(loadedTransform->rotationRadians == Catch::Approx(0.25f));
    REQUIRE(loadedTransform->scale.x == Catch::Approx(2.0f));
    REQUIRE(loadedTransform->scale.y == Catch::Approx(3.0f));
    REQUIRE(loadedTransform->worldPosition.x == Catch::Approx(0.0f));
    REQUIRE(loadedTransform->dirty);

    const auto* camera =
        loaded->GetComponent<Janus::CameraComponent>(loadedRoot);
    REQUIRE(camera != nullptr);
    REQUIRE(camera->zoom == Catch::Approx(1.5f));
    REQUIRE(camera->primary);

    const auto* sprite =
        loaded->GetComponent<Janus::SpriteRendererComponent>(loadedChild);
    REQUIRE(sprite != nullptr);
    REQUIRE(sprite->texture == texture);
    REQUIRE(sprite->size.x == Catch::Approx(128.0f));
    REQUIRE(sprite->size.y == Catch::Approx(64.0f));
    REQUIRE(sprite->color.r == Catch::Approx(0.2f));
    REQUIRE(sprite->color.a == Catch::Approx(0.8f));
    REQUIRE(sprite->layer == 7);
    REQUIRE(sprite->uv.min.x == Catch::Approx(0.1f));
    REQUIRE(sprite->uv.max.y == Catch::Approx(0.9f));
    REQUIRE_FALSE(sprite->enabled);

    REQUIRE(loaded->GetComponent<Janus::HierarchyComponent>(loadedChild)->parent
            == loadedRoot);
    REQUIRE(loaded->GetComponent<Janus::HierarchyComponent>(loadedGrandchild)->parent
            == loadedChild);

    auto second = Janus::SceneSerializer::Serialize(*loaded);
    REQUIRE(second);
    REQUIRE(second.Value() == text);
}

TEST_CASE("Scene serializer round trips an empty Scene and file IO",
          "[scene][serialization]")
{
    Janus::Scene scene(Janus::SceneMetadata{
        ParseUUID("11000000-0000-4000-8000-000000000001"),
        "Empty"});
    SceneTempDirectory temp;
    const auto path = temp.Path() / "Empty.scene";

    REQUIRE(Janus::SceneSerializer::Save(scene, path));
    auto loaded = Janus::SceneDeserializer::Load(path);
    REQUIRE(loaded);
    REQUIRE(loaded.Value()->GetMetadata().name == "Empty");
    REQUIRE(loaded.Value()->GetEntities().empty());
}

TEST_CASE("Scene deserializer resolves a parent declared after its child",
          "[scene][serialization]")
{
    const std::string text = R"json({
  "schema": "janus.scene",
  "version": 1,
  "scene": {
    "id": "12000000-0000-4000-8000-000000000001",
    "name": "OutOfOrder"
  },
  "entities": [
    {
      "id": "22000000-0000-4000-8000-000000000002",
      "name": "Child",
      "parent": "22000000-0000-4000-8000-000000000001",
      "siblingOrder": 0,
      "components": {
        "Transform": {
          "position": [1.0, 2.0],
          "rotation": 0.0,
          "scale": [1.0, 1.0]
        }
      }
    },
    {
      "id": "22000000-0000-4000-8000-000000000001",
      "name": "Parent",
      "parent": null,
      "siblingOrder": 0,
      "components": {
        "Transform": {
          "position": [0.0, 0.0],
          "rotation": 0.0,
          "scale": [1.0, 1.0]
        }
      }
    }
  ]
})json";

    auto loaded = Janus::SceneDeserializer::Deserialize(text);
    REQUIRE(loaded);

    const auto parent = loaded.Value()->FindEntity(
        ParseUUID("22000000-0000-4000-8000-000000000001"));
    const auto child = loaded.Value()->FindEntity(
        ParseUUID("22000000-0000-4000-8000-000000000002"));
    REQUIRE(loaded.Value()->GetComponent<Janus::HierarchyComponent>(child)->parent
            == parent);
}

TEST_CASE("Scene deserializer rejects duplicate and missing hierarchy identities",
          "[scene][serialization]")
{
    SECTION("duplicate entity UUID")
    {
        const std::string text = R"json({
          "schema":"janus.scene","version":1,
          "scene":{"id":"13000000-0000-4000-8000-000000000001","name":"Bad"},
          "entities":[
            {"id":"23000000-0000-4000-8000-000000000001","name":"A","parent":null,"components":{"Transform":{"position":[0,0],"rotation":0,"scale":[1,1]}}},
            {"id":"23000000-0000-4000-8000-000000000001","name":"B","parent":null,"components":{"Transform":{"position":[0,0],"rotation":0,"scale":[1,1]}}}
          ]
        })json";
        REQUIRE_FALSE(Janus::SceneDeserializer::Deserialize(text));
    }

    SECTION("missing parent UUID")
    {
        const std::string text = R"json({
          "schema":"janus.scene","version":1,
          "scene":{"id":"13000000-0000-4000-8000-000000000002","name":"Bad"},
          "entities":[
            {"id":"23000000-0000-4000-8000-000000000002","name":"Child","parent":"23000000-0000-4000-8000-000000000099","components":{"Transform":{"position":[0,0],"rotation":0,"scale":[1,1]}}}
          ]
        })json";
        const auto loaded = Janus::SceneDeserializer::Deserialize(text);
        REQUIRE_FALSE(loaded);
        REQUIRE(loaded.GetError().code == Janus::ErrorCode::EntityNotFound);
    }
}

TEST_CASE("Scene deserializer rejects invalid persistent references and schema",
          "[scene][serialization]")
{
    SECTION("invalid entity UUID")
    {
        const std::string text = R"json({
          "schema":"janus.scene","version":1,
          "scene":{"id":"14000000-0000-4000-8000-000000000001","name":"Bad"},
          "entities":[{"id":"not-a-uuid","name":"Entity","parent":null,"components":{"Transform":{"position":[0,0],"rotation":0,"scale":[1,1]}}}]
        })json";
        REQUIRE_FALSE(Janus::SceneDeserializer::Deserialize(text));
    }

    SECTION("invalid AssetHandle")
    {
        const std::string text = R"json({
          "schema":"janus.scene","version":1,
          "scene":{"id":"14000000-0000-4000-8000-000000000002","name":"Bad"},
          "entities":[{"id":"24000000-0000-4000-8000-000000000001","name":"Sprite","parent":null,"components":{
            "Transform":{"position":[0,0],"rotation":0,"scale":[1,1]},
            "SpriteRenderer":{"texture":"bad-handle","size":[1,1],"color":[1,1,1,1],"layer":0,"uvMin":[0,0],"uvMax":[1,1],"enabled":true}
          }}]
        })json";
        REQUIRE_FALSE(Janus::SceneDeserializer::Deserialize(text));
    }

    SECTION("unsupported version")
    {
        const std::string text = R"json({
          "schema":"janus.scene","version":2,
          "scene":{"id":"14000000-0000-4000-8000-000000000003","name":"Future"},
          "entities":[]
        })json";
        REQUIRE_FALSE(Janus::SceneDeserializer::Deserialize(text));
    }

    SECTION("unsupported schema")
    {
        const std::string text = R"json({
          "schema":"other.scene","version":1,
          "scene":{"id":"14000000-0000-4000-8000-000000000004","name":"Other"},
          "entities":[]
        })json";
        REQUIRE_FALSE(Janus::SceneDeserializer::Deserialize(text));
    }

    SECTION("malformed JSON")
    {
        REQUIRE_FALSE(Janus::SceneDeserializer::Deserialize("{broken"));
    }
}
