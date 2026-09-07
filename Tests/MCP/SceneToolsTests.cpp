#include "Animation/AnimatorComponent.h"
#include "Tools/SceneTools.h"

#include "Asset/AssetRegistry.h"
#include "Core/Command/CommandBus.h"
#include "Registry/ToolRegistry.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <variant>

namespace
{

struct ToolFixture
{
    Janus::ReflectionRegistry reflection;
    Janus::Scene scene;
    Janus::AssetRegistry assets;
    Janus::CommandBus commands;
    Janus::MCP::ToolRegistry tools;

    bool dirty = false;
    bool readOnly = false;
    bool saved = false;

    ToolFixture()
    {
        auto reflected =
            Janus::CreateBuiltinSceneReflectionRegistry();
        REQUIRE(reflected);

        reflection =
            std::move(reflected).Value();

        REQUIRE(
            Janus::MCP::RegisterSceneTools(
                tools,
                Janus::MCP::McpSceneToolContext{
                    &scene,
                    &reflection,
                    &commands,
                    &assets,
                    [this]()
                    {
                        saved = true;
                        return Janus::Result<void>::Success();
                    },
                    [this]()
                    {
                        dirty = true;
                    },
                    [this]()
                    {
                        return readOnly;
                    }}));
    }
};

const Janus::MCP::Json& RequireJson(
    const Janus::MCP::McpDispatchResult& result)
{
    REQUIRE(
        std::holds_alternative<Janus::MCP::Json>(
            result));
    return std::get<Janus::MCP::Json>(
        result);
}

const Janus::MCP::McpDispatchError& RequireDispatchError(
    const Janus::MCP::McpDispatchResult& result)
{
    REQUIRE(
        std::holds_alternative<Janus::MCP::McpDispatchError>(
            result));
    return std::get<Janus::MCP::McpDispatchError>(
        result);
}

Janus::MCP::Json CallTool(
    ToolFixture& fixture,
    std::string name,
    Janus::MCP::Json arguments =
        Janus::MCP::Json::object())
{
    const auto result =
        fixture.tools.HandleCall(
            Janus::MCP::Json{
                {"name", std::move(name)},
                {"arguments",
                 std::move(arguments)}},
            Janus::MCP::McpProtocolEra::Modern2026);

    return RequireJson(result);
}

Janus::UUID StructuredEntity(
    const Janus::MCP::Json& result)
{
    const auto parsed =
        Janus::UUID::Parse(
            result.at("structuredContent")
                .at("entity")
                .get<std::string>());

    REQUIRE(parsed);
    return parsed.Value();
}

} // namespace

TEST_CASE("MCP reparent shares undo history rejects invalid input and honors read-only",
          "[ui][mcp]")
{
    ToolFixture fixture;
    const auto child =
        StructuredEntity(CallTool(fixture, "scene.create_entity", {{"name", "Child"}}));
    const auto parent =
        StructuredEntity(CallTool(fixture, "scene.create_entity", {{"name", "Parent"}}));
    fixture.dirty = false;
    auto result = CallTool(
        fixture, "scene.reparent_entity",
        {{"entity", child.ToString()}, {"parent", parent.ToString()}, {"siblingIndex", 0}});
    REQUIRE(result.at("structuredContent").at("ok") == true);
    CHECK(fixture.dirty);
    auto entity = fixture.scene.FindEntity(child);
    CHECK(fixture.scene.GetComponent<Janus::HierarchyComponent>(entity)->parent ==
          fixture.scene.FindEntity(parent));
    REQUIRE(fixture.commands.Undo());
    CHECK_FALSE(fixture.scene.GetComponent<Janus::HierarchyComponent>(entity)->parent.IsValid());
    REQUIRE(fixture.commands.Redo());
    fixture.readOnly = true;
    auto denied = CallTool(fixture, "scene.reparent_entity",
                           {{"entity", child.ToString()}, {"parent", nullptr}});
    CHECK(denied.at("isError") == true);
    fixture.readOnly = false;
    const auto invalid = fixture.tools.HandleCall(
        {{"name", "scene.reparent_entity"},
         {"arguments", {{"entity", child.ToString()}, {"parent", nullptr}, {"siblingIndex", -1}}}},
        Janus::MCP::McpProtocolEra::Modern2026);
    RequireDispatchError(invalid);
    REQUIRE(CallTool(fixture, "scene.reparent_entity",
                     {{"entity", child.ToString()}, {"parent", nullptr}})
                .at("structuredContent")
                .at("ok") == true);
}

TEST_CASE(
    "Scene MCP tool registration exposes the v0.8 command set",
    "[mcp][tool][scene][v0.8]")
{
    ToolFixture fixture;

    REQUIRE(fixture.tools.GetToolCount() == 9);
    REQUIRE(
        fixture.tools.FindTool(
            "scene.create_entity")
        != nullptr);
    REQUIRE(
        fixture.tools.FindTool(
            "scene.set_component_property")
        != nullptr);
    REQUIRE(
        fixture.tools.FindTool(
            "scene.save")
        != nullptr);

    const auto* setProperty =
        fixture.tools.FindTool(
            "scene.set_component_property");

    REQUIRE(setProperty != nullptr);
    REQUIRE(
        setProperty->inputSchema.at("type")
        == "object");
    REQUIRE(
        setProperty->inputSchema
            .at("oneOf")
            .size()
        > 0);

    REQUIRE(
        setProperty->outputSchema.has_value());
    REQUIRE(
        setProperty->outputSchema
            ->at("oneOf")
            .size()
        == 2);
}

TEST_CASE(
    "Scene MCP create enters shared CommandBus history and is undoable",
    "[mcp][tool][scene][v0.8]")
{
    ToolFixture fixture;

    const Janus::MCP::Json result =
        CallTool(
            fixture,
            "scene.create_entity",
            Janus::MCP::Json{
                {"name", "AgentEntity"}});

    REQUIRE(
        result.at("structuredContent")
            .at("ok")
        == true);

    const Janus::UUID entity =
        StructuredEntity(result);

    const Janus::ECS::Entity runtime =
        fixture.scene.FindEntity(entity);

    REQUIRE(runtime.IsValid());
    REQUIRE(
        fixture.scene
            .GetComponent<Janus::EntityIdentityComponent>(
                runtime)
            ->name
        == "AgentEntity");
    REQUIRE(fixture.commands.GetHistorySize() == 1);
    REQUIRE(fixture.dirty);

    REQUIRE(fixture.commands.Undo());
    REQUIRE_FALSE(
        fixture.scene.FindEntity(entity)
            .IsValid());
}

TEST_CASE(
    "Scene MCP rename and delete use reversible entity commands",
    "[mcp][tool][scene][v0.8]")
{
    ToolFixture fixture;

    const Janus::ECS::Entity entity =
        fixture.scene.CreateEntity("Before");
    const Janus::UUID id =
        fixture.scene
            .GetComponent<Janus::EntityIdentityComponent>(
                entity)
            ->id;

    const Janus::MCP::Json renamed =
        CallTool(
            fixture,
            "scene.rename_entity",
            Janus::MCP::Json{
                {"entity", id.ToString()},
                {"name", "After"}});

    REQUIRE(
        renamed.at("structuredContent")
            .at("ok")
        == true);
    REQUIRE(
        fixture.scene
            .GetComponent<Janus::EntityIdentityComponent>(
                entity)
            ->name
        == "After");

    REQUIRE(fixture.commands.Undo());
    REQUIRE(
        fixture.scene
            .GetComponent<Janus::EntityIdentityComponent>(
                entity)
            ->name
        == "Before");

    const Janus::MCP::Json deleted =
        CallTool(
            fixture,
            "scene.delete_entity",
            Janus::MCP::Json{
                {"entity", id.ToString()}});

    REQUIRE(
        deleted.at("structuredContent")
            .at("ok")
        == true);
    REQUIRE_FALSE(
        fixture.scene.FindEntity(id)
            .IsValid());

    REQUIRE(fixture.commands.Undo());
    REQUIRE(
        fixture.scene.FindEntity(id)
            .IsValid());
}

TEST_CASE(
    "Scene MCP add and remove component commands share history",
    "[mcp][tool][scene][v0.8]")
{
    ToolFixture fixture;

    const Janus::ECS::Entity entity =
        fixture.scene.CreateEntity("Entity");
    const Janus::UUID id =
        fixture.scene
            .GetComponent<Janus::EntityIdentityComponent>(
                entity)
            ->id;

    REQUIRE_FALSE(
        fixture.scene.HasComponent<Janus::CameraComponent>(
            entity));

    const Janus::MCP::Json added =
        CallTool(
            fixture,
            "scene.add_component",
            Janus::MCP::Json{
                {"entity", id.ToString()},
                {"component", "Camera"}});

    REQUIRE(
        added.at("structuredContent")
            .at("ok")
        == true);
    REQUIRE(
        fixture.scene.HasComponent<Janus::CameraComponent>(
            entity));

    REQUIRE(fixture.commands.Undo());
    REQUIRE_FALSE(
        fixture.scene.HasComponent<Janus::CameraComponent>(
            entity));

    fixture.scene.AddComponent<Janus::CameraComponent>(
        entity,
        Janus::CameraComponent{});

    const Janus::MCP::Json removed =
        CallTool(
            fixture,
            "scene.remove_component",
            Janus::MCP::Json{
                {"entity", id.ToString()},
                {"component", "Camera"}});

    REQUIRE(
        removed.at("structuredContent")
            .at("ok")
        == true);
    REQUIRE_FALSE(
        fixture.scene.HasComponent<Janus::CameraComponent>(
            entity));

    REQUIRE(fixture.commands.Undo());
    REQUIRE(
        fixture.scene.HasComponent<Janus::CameraComponent>(
            entity));
}

TEST_CASE(
    "Scene MCP reflected property mutation is typed and undoable",
    "[mcp][tool][scene][v0.8]")
{
    ToolFixture fixture;

    const Janus::ECS::Entity entity =
        fixture.scene.CreateEntity("Entity");
    const Janus::UUID id =
        fixture.scene
            .GetComponent<Janus::EntityIdentityComponent>(
                entity)
            ->id;

    auto* transform =
        fixture.scene.GetComponent<Janus::TransformComponent>(
            entity);
    REQUIRE(transform != nullptr);

    const Janus::MCP::Json moved =
        CallTool(
            fixture,
            "scene.set_component_property",
            Janus::MCP::Json{
                {"entity", id.ToString()},
                {"component", "Transform"},
                {"property", "position"},
                {"value",
                 Janus::MCP::Json{
                     {"x", 4.0},
                     {"y", 2.0}}}});

    REQUIRE(
        moved.at("structuredContent")
            .at("ok")
        == true);
    REQUIRE(transform->position.x == 4.0f);
    REQUIRE(transform->position.y == 2.0f);

    REQUIRE(fixture.commands.Undo());
    REQUIRE(transform->position.x == 0.0f);
    REQUIRE(transform->position.y == 0.0f);

    const Janus::usize historyBefore =
        fixture.commands.GetHistorySize();

    const auto invalid =
        fixture.tools.HandleCall(
            Janus::MCP::Json{
                {"name",
                 "scene.set_component_property"},
                {"arguments",
                 Janus::MCP::Json{
                     {"entity", id.ToString()},
                     {"component", "Transform"},
                     {"property", "position"},
                     {"value", "wrong"}}}},
            Janus::MCP::McpProtocolEra::Modern2026);

    REQUIRE(
        RequireDispatchError(invalid).code
        == Janus::MCP::JsonRpcInvalidParams);
    REQUIRE(
        fixture.commands.GetHistorySize()
        == historyBefore);
}

TEST_CASE(
    "Scene MCP Camera primary mutation preserves contextual undo semantics",
    "[mcp][tool][scene][v0.8]")
{
    ToolFixture fixture;

    const Janus::ECS::Entity first =
        fixture.scene.CreateEntity("First");
    const Janus::ECS::Entity second =
        fixture.scene.CreateEntity("Second");

    REQUIRE(
        fixture.scene.AddComponent<Janus::CameraComponent>(
            first,
            Janus::CameraComponent{
                1.0f,
                true})
        != nullptr);
    REQUIRE(
        fixture.scene.AddComponent<Janus::CameraComponent>(
            second,
            Janus::CameraComponent{
                1.0f,
                false})
        != nullptr);

    auto* firstCamera =
        fixture.scene.GetComponent<Janus::CameraComponent>(
            first);
    auto* secondCamera =
        fixture.scene.GetComponent<Janus::CameraComponent>(
            second);

    REQUIRE(firstCamera != nullptr);
    REQUIRE(secondCamera != nullptr);

    const Janus::UUID secondId =
        fixture.scene
            .GetComponent<Janus::EntityIdentityComponent>(
                second)
            ->id;

    const Janus::MCP::Json result =
        CallTool(
            fixture,
            "scene.set_component_property",
            Janus::MCP::Json{
                {"entity", secondId.ToString()},
                {"component", "Camera"},
                {"property", "primary"},
                {"value", true}});

    REQUIRE(
        result.at("structuredContent")
            .at("ok")
        == true);
    REQUIRE_FALSE(firstCamera->primary);
    REQUIRE(secondCamera->primary);

    REQUIRE(fixture.commands.Undo());
    REQUIRE(firstCamera->primary);
    REQUIRE_FALSE(secondCamera->primary);
}

TEST_CASE(
    "Scene MCP AssetReference validation preserves type safety",
    "[mcp][tool][scene][v0.8]")
{
    ToolFixture fixture;

    const Janus::ECS::Entity entity =
        fixture.scene.CreateEntity("Scripted");

    fixture.scene.AddComponent<Janus::LuaScriptComponent>(
        entity,
        Janus::LuaScriptComponent{});

    const Janus::UUID id =
        fixture.scene
            .GetComponent<Janus::EntityIdentityComponent>(
                entity)
            ->id;

    const Janus::AssetHandle texture =
        Janus::AssetHandle::Random();

    REQUIRE(
        fixture.assets.Register(
            Janus::AssetMetadata{
                texture,
                Janus::AssetType::Texture,
                "Textures/not-a-script.png"}));

    const Janus::usize historyBefore =
        fixture.commands.GetHistorySize();

    const Janus::MCP::Json result =
        CallTool(
            fixture,
            "scene.set_component_property",
            Janus::MCP::Json{
                {"entity", id.ToString()},
                {"component", "LuaScript"},
                {"property", "script"},
                {"value", texture.ToString()}});

    REQUIRE(
        result.at("isError")
        == true);
    REQUIRE(
        result.at("structuredContent")
            .at("ok")
        == false);
    REQUIRE(
        fixture.commands.GetHistorySize()
        == historyBefore);
}

TEST_CASE(
    "Scene MCP write guard and save callback are host injected",
    "[mcp][tool][scene][v0.8]")
{
    ToolFixture fixture;

    fixture.readOnly = true;

    const Janus::MCP::Json denied =
        CallTool(
            fixture,
            "scene.create_entity",
            Janus::MCP::Json{
                {"name", "Denied"}});

    REQUIRE(
        denied.at("isError")
        == true);
    REQUIRE(
        fixture.commands.GetHistorySize()
        == 0);

    const Janus::MCP::Json deniedSave =
        CallTool(
            fixture,
            "scene.save");

    REQUIRE(
        deniedSave.at("isError")
        == true);
    REQUIRE_FALSE(fixture.saved);

    fixture.readOnly = false;

    const Janus::MCP::Json saved =
        CallTool(
            fixture,
            "scene.save");

    REQUIRE(
        saved.at("structuredContent")
            .at("ok")
        == true);
    REQUIRE(fixture.saved);
}

TEST_CASE("MCP asset search is read-only typed bounded and rejects invalid parameters",
          "[ui][text][mcp]")
{
    ToolFixture fixture;
    REQUIRE(fixture.assets.Register(Janus::AssetType::Font, "Fonts/Menu.json"));
    REQUIRE(fixture.assets.Register(Janus::AssetType::Texture, "Fonts/Menu.png"));
    fixture.readOnly = true;
    const auto result =
        CallTool(fixture, "assets.search", {{"name", "Menu"}, {"type", "font"}, {"limit", 1}});
    const auto& data = result.at("structuredContent");
    CHECK(data.at("total") == 1);
    REQUIRE(data.at("assets").size() == 1);
    CHECK(data.at("assets")[0].at("path") == "Fonts/Menu.json");
    CHECK_FALSE(fixture.dirty);
    for (auto arguments : {Janus::MCP::Json{{"limit", -1}}, Janus::MCP::Json{{"limit", 101}},
                           Janus::MCP::Json{{"offset", 1.5}}, Janus::MCP::Json{{"type", "typo"}},
                           Janus::MCP::Json{{"name", 12}}, Janus::MCP::Json{{"transaction", "x"}}})
    {
        auto invalid =
            fixture.tools.HandleCall({{"name", "assets.search"}, {"arguments", arguments}},
                                     Janus::MCP::McpProtocolEra::Modern2026);
        RequireDispatchError(invalid);
    }
}

TEST_CASE("MCP discovers animation clips and authors Animator through guarded commands",
          "[animation][mcp]")
{
    ToolFixture fixture;
    auto clip = fixture.assets.Register(Janus::AssetType::AnimationClip, "Animations/Pulse.json");
    REQUIRE(clip);
    auto texture = fixture.assets.Register(Janus::AssetType::Texture, "Animations/Atlas.png");
    REQUIRE(texture);
    const auto search = CallTool(fixture, "assets.search", {{"type", "animation-clip"}});
    REQUIRE(search.at("structuredContent").at("assets").size() == 1);
    CHECK_FALSE(fixture.dirty);
    const auto entity = fixture.scene.CreateEntity("Animated");
    const auto id =
        fixture.scene.GetComponent<Janus::EntityIdentityComponent>(entity)->id.ToString();
    REQUIRE(CallTool(fixture, "scene.add_component", {{"entity", id}, {"component", "Animator"}})
                .at("structuredContent")
                .at("ok") == true);
    auto set = [&](const char* property, Janus::MCP::Json value)
    {
        return CallTool(
            fixture, "scene.set_component_property",
            {{"entity", id}, {"component", "Animator"}, {"property", property}, {"value", value}});
    };
    REQUIRE(set("clip", texture.Value().ToString()).at("isError") == true);
    REQUIRE(set("clip", clip.Value().ToString()).at("structuredContent").at("ok") == true);
    REQUIRE(set("enabled", true).at("structuredContent").at("ok") == true);
    REQUIRE(fixture.commands.Undo());
    CHECK_FALSE(fixture.scene.GetComponent<Janus::AnimatorComponent>(entity)->enabled);
    fixture.readOnly = true;
    REQUIRE(set("speed", 2).at("isError") == true);
    CHECK(fixture.scene.GetComponent<Janus::AnimatorComponent>(entity)->speed == 1);
}
