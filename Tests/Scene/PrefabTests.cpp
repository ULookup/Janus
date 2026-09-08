#include "../Asset/AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"
#include "Animation/AnimatorComponent.h"
#include "Asset/AssetRegistry.h"
#include "Audio/AudioSourceComponent.h"
#include "Core/Command/CommandBus.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "EditorActions.h"
#include "EditorContext.h"
#include "Host/McpPermissionPolicy.h"
#include "Physics/PhysicsComponents.h"
#include "Prefab/Prefab.h"
#include "ProjectSession.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneSerializer.h"
#include "Tools/SceneTools.h"
#include "UI/UIComponents.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <set>

using namespace Janus;

TEST_CASE("Prefab display names reject unsafe fragments without sanitizing",
          "[prefab][asset-workflow]")
{
    for (const std::string name :
         {"", ".", "..", "../Robot", "a/b", "a\\b", "a:b", "x?", "CON", "com1.txt", "LPT9", "aux",
          "NUL.txt", "a.", "a ", "a\n", "CONIN$"})
    {
        INFO(name);
        REQUIRE_FALSE(Editor::ProjectSession::ValidatePrefabName(name));
    }
    REQUIRE_FALSE(Editor::ProjectSession::ValidatePrefabName(std::string("a\0b", 3)));
    REQUIRE_FALSE(Editor::ProjectSession::ValidatePrefabName("\xc0\xaf"));
    REQUIRE_FALSE(Editor::ProjectSession::ValidatePrefabName("name\xc2\x85"));
    REQUIRE_FALSE(Editor::ProjectSession::ValidatePrefabName(std::string(97, 'a')));
    REQUIRE(Editor::ProjectSession::ValidatePrefabName("Robot Fighter"));
    REQUIRE(Editor::ProjectSession::ValidatePrefabName("\xe6\x9c\xba\xe5\x99\xa8\xe4\xba\xba"));
}

TEST_CASE("Prefab authoring shares Human Agent history and guards disk exports",
          "[prefab][editor][mcp]")
{
    Test::AssetTempDirectory temp;
    std::filesystem::create_directories(temp.Path() / "Scenes");
    std::filesystem::create_directories(temp.Path() / "Config");
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    Scene source;
    auto entity = source.CreateEntity("Template");
    const auto id = source.GetComponent<EntityIdentityComponent>(entity)->id;
    REQUIRE(SceneSerializer::Save(source, registry.Value(), temp.Path() / "Scenes/Battle.scene"));
    REQUIRE(AssetRegistry{}.Save(temp.Path() / "Config/AssetRegistry.json"));
    Test::FakeRenderDevice device;
    auto renderer = Detail::Renderer2DTestAccess::Create(device);
    ProjectRuntimeConfig config;
    config.root = temp.Path();
    auto opened = Editor::ProjectSession::Open(config, *renderer);
    REQUIRE(opened);
    auto& project = *opened.Value();
    Editor::EditorContext context;
    context.project = &project;
    Editor::EditorActions actions(context);
    auto asset = actions.ExportPrefab(id);
    REQUIRE(asset);
    REQUIRE_FALSE(project.IsDirty());
    REQUIRE(project.GetCommandBus().GetHistorySize() == 0);
    REQUIRE(project.GetAssetRegistry().Search({}, AssetType::Prefab).Value().total == 1);
    auto diskAssets = AssetRegistry::Load(temp.Path() / "Config/AssetRegistry.json");
    REQUIRE(diskAssets);
    REQUIRE(diskAssets.Value().Contains(asset.Value()));
    auto created = actions.InstantiatePrefab(asset.Value());
    REQUIRE(created);
    REQUIRE(project.IsDirty());
    REQUIRE(actions.Undo());
    REQUIRE(project.GetEditorScene().GetEntities().size() == 1);
    REQUIRE(actions.Redo());
    REQUIRE(project.SaveCurrentScene());
    REQUIRE(project.PlayRuntime(true));
    REQUIRE_FALSE(actions.InstantiatePrefab(asset.Value()));
    REQUIRE_FALSE(actions.ExportPrefab(id));
    REQUIRE_FALSE(actions.DuplicateEntity(id));
    REQUIRE(project.StepRuntime());
    REQUIRE(project.GetEditorScene().GetEntities().size() == 2);
    REQUIRE(project.StopRuntime());
    REQUIRE_FALSE(project.IsDirty());

    UUID owner = UUID::Random();
    MCP::ToolRegistry tools;
    MCP::McpSceneToolContext toolContext{
        &project.GetEditorScene(),
        &project.GetReflectionRegistry(),
        &project.GetCommandBus(),
        &project.GetAssetRegistry(),
        [&] { return project.SaveCurrentScene(); },
        [&] { project.MarkDirty(); },
        [&] { return project.HasRuntime(); },
        [&](std::unique_ptr<ICommand> command, UUID token)
        { return project.ExecuteAuthoring(std::move(command), CommandActor::Agent, token, owner); },
        temp.Path(),
        [&](UUID root, std::optional<std::string> name)
        { return project.ExportPrefab(root, std::move(name)); }};
    REQUIRE(MCP::RegisterSceneTools(tools, toolContext));
    auto call = [&](std::string name, MCP::Json args)
    {
        auto result = tools.HandleCall({{"name", name}, {"arguments", args}},
                                       MCP::McpProtocolEra::Modern2026);
        REQUIRE(std::holds_alternative<MCP::Json>(result));
        return std::get<MCP::Json>(result);
    };
    auto token = project.BeginAuthoringTransaction(owner);
    REQUIRE(token);
    REQUIRE_FALSE(actions.ExportPrefab(id));
    REQUIRE_FALSE(actions.InstantiatePrefab(asset.Value()));
    auto blocked = call("scene.instantiate_prefab", {{"asset", asset.Value().ToString()}});
    REQUIRE(blocked.value("isError", false));
    auto ok = call("scene.instantiate_prefab", {{"asset", asset.Value().ToString()},
                                                {"transaction", token.Value().ToString()}});
    REQUIRE_FALSE(ok.value("isError", false));
    REQUIRE(project.GetEditorScene().GetEntities().size() == 3);
    REQUIRE(project.FinishAuthoringTransaction(token.Value(), owner, false));
    REQUIRE(project.GetEditorScene().GetEntities().size() == 2);
    REQUIRE_FALSE(project.IsDirty());
    auto exported = call("scene.export_prefab", {{"entity", id.ToString()}});
    REQUIRE_FALSE(exported.value("isError", false));
    REQUIRE_FALSE(project.IsDirty());
    REQUIRE(MCP::ClassifyMcpOperation("tools/call", {{"name", "scene.export_prefab"}}) ==
            MCP::McpOperation::SceneSave);
    REQUIRE(MCP::ClassifyMcpOperation("tools/call", {{"name", "scene.instantiate_prefab"}}) ==
            MCP::McpOperation::SceneWrite);
    REQUIRE(MCP::ClassifyMcpOperation("tools/call", {{"name", "scene.duplicate_entity"}}) ==
            MCP::McpOperation::SceneWrite);
    auto duplicateToken = project.BeginAuthoringTransaction(owner);
    REQUIRE(duplicateToken);
    REQUIRE(call("scene.duplicate_entity", {{"entity", id.ToString()}}).value("isError", false));
    REQUIRE(project.GetCommandBus().HasTransaction());
    REQUIRE_FALSE(
        call("scene.duplicate_entity",
             {{"entity", id.ToString()}, {"transaction", duplicateToken.Value().ToString()}})
            .value("isError", false));
    REQUIRE(project.FinishAuthoringTransaction(duplicateToken.Value(), owner, false));
    REQUIRE(project.GetEditorScene().GetEntities().size() == 2);
    REQUIRE_FALSE(project.IsDirty());
    auto invalid = tools.HandleCall(
        {{"name", "scene.export_prefab"},
         {"arguments", {{"entity", id.ToString()}, {"transaction", UUID::Random().ToString()}}}},
        MCP::McpProtocolEra::Modern2026);
    REQUIRE(std::holds_alternative<MCP::McpDispatchError>(invalid));
    auto reopened = Editor::ProjectSession::Open(config, *renderer);
    REQUIRE(reopened);
    REQUIRE(reopened.Value()->GetEditorScene().GetEntities().size() == 2);
    REQUIRE(reopened.Value()->GetAssetRegistry().Contains(asset.Value()));

    class BrokenUndo final : public ICommand
    {
      public:
        Result<void> Execute() override
        {
            return Result<void>::Success();
        }
        Result<void> Undo() override
        {
            return Result<void>::Failure(ErrorCode::InvalidState, "Injected undo failure.");
        }
        Result<void> Redo() override
        {
            return Execute();
        }
        Result<usize> EstimateUndoBytes() const override
        {
            return Result<usize>::Success(64);
        }
        std::string_view Describe() const noexcept override
        {
            return "Broken undo fixture";
        }
    };
    token = project.BeginAuthoringTransaction(owner);
    REQUIRE(token);
    REQUIRE(project.ExecuteAuthoring(std::make_unique<BrokenUndo>(), CommandActor::Agent,
                                     token.Value(), owner));
    REQUIRE_FALSE(project.FinishAuthoringTransaction(token.Value(), owner, false));
    REQUIRE(project.GetCommandBus().RecoveryRequired());
    REQUIRE_FALSE(actions.ExportPrefab(id));
    REQUIRE_FALSE(actions.InstantiatePrefab(asset.Value()));
    REQUIRE(project.DiscardUnsavedAndReload());

    const auto assetCount = project.GetAssetRegistry().Size();
    std::filesystem::rename(temp.Path() / "Config/AssetRegistry.json",
                            temp.Path() / "Config/Backup.json");
    std::filesystem::create_directory(temp.Path() / "Config/AssetRegistry.json");
    auto countFiles = [&]
    {
        return std::distance(std::filesystem::directory_iterator(temp.Path() / "Prefabs"),
                             std::filesystem::directory_iterator{});
    };
    const auto fileCount = countFiles();
    REQUIRE_FALSE(actions.ExportPrefab(id));
    REQUIRE(project.GetAssetRegistry().Size() == assetCount);
    REQUIRE(countFiles() == fileCount);
    REQUIRE_FALSE(project.IsDirty());
}

TEST_CASE("Prefab expands subtrees with fresh identities and stable undo redo", "[prefab]")
{
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    auto& reflection = registry.Value();
    Scene source;
    auto outside = source.CreateEntity("Outside");
    auto root = source.CreateEntity("Template");
    auto first = source.CreateEntity("First");
    auto second = source.CreateEntity("Second");
    REQUIRE(source.SetParent(root, outside));
    REQUIRE(source.SetParent(second, root));
    REQUIRE(source.SetParent(first, root));
    UUID rootId = source.GetComponent<EntityIdentityComponent>(root)->id;
    AssetHandle asset{UUID::Random()};
    SpriteRendererComponent sprite;
    sprite.texture = asset;
    REQUIRE(source.AddComponent(root, sprite));
    REQUIRE(source.AddComponent(root, AnimatorComponent{}));
    LuaScriptComponent script;
    script.script = asset;
    REQUIRE(source.AddComponent(first, script));
    auto text = Prefab::Capture(source, rootId, reflection);
    REQUIRE(text);
    auto parsed = Prefab::Parse(text.Value(), reflection);
    REQUIRE(parsed);
    REQUIRE(parsed.Value().entities.size() == 3);
    REQUIRE_FALSE(parsed.Value().entities.front().parent.has_value());
    Scene target;
    CommandBus commands;
    std::set<UUID> ids;
    UUID last;
    for (int i = 0; i != 2; ++i)
    {
        auto command = InstantiatePrefabCommand::Create(target, reflection, text.Value());
        REQUIRE(command);
        last = command.Value()->GetRoot();
        REQUIRE(last != rootId);
        REQUIRE(commands.Execute(std::move(command).Value()));
    }
    for (auto entity : target.GetEntities())
        REQUIRE(ids.insert(target.GetComponent<EntityIdentityComponent>(entity)->id).second);
    REQUIRE(ids.size() == 6);
    auto instance = target.FindEntity(last);
    REQUIRE(target.GetComponent<SpriteRendererComponent>(instance)->texture == asset);
    REQUIRE(target.HasComponent<AnimatorComponent>(instance));
    auto child = target.GetComponent<HierarchyComponent>(instance)->firstChild;
    REQUIRE(target.GetComponent<EntityIdentityComponent>(child)->name == "First");
    REQUIRE(target.GetComponent<LuaScriptComponent>(child)->script == asset);
    child = target.GetComponent<HierarchyComponent>(child)->nextSibling;
    REQUIRE(target.GetComponent<EntityIdentityComponent>(child)->name == "Second");
    auto before = SceneSerializer::Serialize(target, reflection);
    REQUIRE(before);
    REQUIRE(commands.Undo());
    REQUIRE(target.GetEntities().size() == 3);
    REQUIRE(commands.Redo());
    REQUIRE(SceneSerializer::Serialize(target, reflection).Value() == before.Value());
    auto loaded = SceneDeserializer::Deserialize(before.Value(), reflection);
    REQUIRE(loaded);
    REQUIRE(loaded.Value()->GetEntities().size() == 6);
}

TEST_CASE("Prefab rejects malformed or unbounded templates without mutation", "[prefab]")
{
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    Scene source;
    auto entity = source.CreateEntity("Root");
    auto text = Prefab::Capture(source, source.GetComponent<EntityIdentityComponent>(entity)->id,
                                registry.Value());
    REQUIRE(text);
    auto valid = nlohmann::json::parse(text.Value());
    Scene target;
    auto reject = [&](nlohmann::json value)
    {
        REQUIRE_FALSE(InstantiatePrefabCommand::Create(target, registry.Value(), value.dump()));
        REQUIRE(target.GetEntities().empty());
    };
    SECTION("version")
    {
        valid["version"] = 2;
        reject(valid);
    }
    SECTION("unknown field")
    {
        valid["overrides"] = true;
        reject(valid);
    }
    SECTION("nested prefab field")
    {
        valid["scene"]["entities"][0]["prefab"] = UUID::Random().ToString();
        reject(valid);
    }
    SECTION("invalid authoring UTF8")
    {
        source.GetComponent<EntityIdentityComponent>(entity)->name =
            std::string(1, static_cast<char>(0xff));
        REQUIRE_FALSE(Prefab::Capture(
            source, source.GetComponent<EntityIdentityComponent>(entity)->id, registry.Value()));
    }
    SECTION("missing root")
    {
        valid["root"] = UUID::Random().ToString();
        reject(valid);
    }
    SECTION("broken parent")
    {
        valid["scene"]["entities"][0]["parent"] = UUID::Random().ToString();
        reject(valid);
    }
    SECTION("cycle")
    {
        valid["scene"]["entities"][0]["parent"] = valid["root"];
        reject(valid);
    }
    SECTION("duplicate entity")
    {
        valid["scene"]["entities"].push_back(valid["scene"]["entities"][0]);
        reject(valid);
    }
    SECTION("multiple roots")
    {
        auto other = valid["scene"]["entities"][0];
        other["id"] = UUID::Random().ToString();
        valid["scene"]["entities"].push_back(other);
        reject(valid);
    }
    SECTION("unknown component")
    {
        valid["scene"]["entities"][0]["components"]["NotRegistered"] = nlohmann::json::object();
        reject(valid);
    }
    SECTION("size")
    {
        REQUIRE_FALSE(Prefab::Parse(std::string(1024 * 1024 + 1, ' '), registry.Value()));
    }
    SECTION("duplicate key")
    {
        REQUIRE_FALSE(Prefab::Parse("{\"schema\":1,\"schema\":2}", registry.Value()));
    }
    SECTION("depth")
    {
        auto parent = entity;
        for (int i = 0; i < 64; ++i)
        {
            auto next = source.CreateEntity();
            REQUIRE(source.SetParent(next, parent));
            parent = next;
        }
        REQUIRE_FALSE(Prefab::Capture(
            source, source.GetComponent<EntityIdentityComponent>(entity)->id, registry.Value()));
    }
}

TEST_CASE("Prefab uses the active registry and compensates restore failures", "[prefab]")
{
    auto builtins = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(builtins);
    ReflectionRegistry reflection;
    bool failRestore = false;
    for (const auto* original : builtins.Value().GetComponents())
    {
        auto descriptor = *original;
        if (descriptor.id == SceneReflectionIds::Transform)
        {
            for (auto& property : descriptor.properties)
                if (property.id == SceneReflectionIds::TransformPosition)
                    property.serializedName = "localOffset";
            auto validate = descriptor.validator;
            descriptor.validator = [&, validate](const void* component)
            {
                if (failRestore)
                    return Result<void>::Failure(ErrorCode::InvalidState,
                                                 "Injected restore failure.");
                return validate ? validate(component) : Result<void>::Success();
            };
        }
        REQUIRE(reflection.RegisterComponent(std::move(descriptor)));
    }
    Scene source;
    auto entity = source.CreateEntity();
    auto child = source.CreateEntity();
    REQUIRE(source.SetParent(child, entity));
    UUID id = source.GetComponent<EntityIdentityComponent>(entity)->id;
    TextComponent text;
    text.content = id.ToString();
    REQUIRE(source.AddComponent(entity, text));
    REQUIRE(source.AddComponent(entity, AudioSourceComponent{}));
    REQUIRE(source.AddComponent(entity, RigidBody2DComponent{}));
    REQUIRE(source.AddComponent(entity, Collider2DComponent{}));
    auto data = Prefab::Capture(source, id, reflection);
    REQUIRE(data);
    REQUIRE(data.Value().find("localOffset") != std::string::npos);
    REQUIRE_FALSE(Prefab::Parse(data.Value(), builtins.Value()));
    Scene target;
    auto existing = target.CreateEntity("Existing");
    UUID existingId = target.GetComponent<EntityIdentityComponent>(existing)->id;
    auto command = InstantiatePrefabCommand::Create(target, reflection, data.Value());
    REQUIRE(command);
    auto newRoot = command.Value()->GetRoot();
    failRestore = true;
    REQUIRE_FALSE(command.Value()->Execute());
    REQUIRE(target.GetEntities().size() == 1);
    REQUIRE(target.FindEntity(existingId).IsValid());
    REQUIRE_FALSE(target.FindEntity(newRoot).IsValid());
    failRestore = false;
    REQUIRE(command.Value()->Execute());
    auto root = target.FindEntity(newRoot);
    REQUIRE(target.GetComponent<TextComponent>(root)->content == id.ToString());
    REQUIRE(target.HasComponent<AudioSourceComponent>(root));
    REQUIRE(target.HasComponent<RigidBody2DComponent>(root));
    REQUIRE(target.HasComponent<Collider2DComponent>(root));
    REQUIRE(command.Value()->Undo());
    REQUIRE(target.CreateEntityWithUUID(newRoot, "Collision"));
    REQUIRE_FALSE(command.Value()->Redo());
    REQUIRE(target.GetEntities().size() == 2);
    REQUIRE(target.GetComponent<EntityIdentityComponent>(target.FindEntity(newRoot))->name ==
            "Collision");
}

TEST_CASE("Prefab rejects conflicting primary cameras and canvases atomically", "[prefab]")
{
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    Scene source, target;
    auto sourceEntity = source.CreateEntity();
    auto targetEntity = target.CreateEntity();
    SECTION("camera")
    {
        REQUIRE(source.AddComponent(sourceEntity, CameraComponent{1, true}));
        REQUIRE(target.AddComponent(targetEntity, CameraComponent{1, true}));
    }
    SECTION("canvas")
    {
        REQUIRE(source.AddComponent(sourceEntity, CanvasComponent{}));
        REQUIRE(target.AddComponent(targetEntity, CanvasComponent{}));
    }
    auto data = Prefab::Capture(
        source, source.GetComponent<EntityIdentityComponent>(sourceEntity)->id, reflection.Value());
    REQUIRE(data);
    auto command = InstantiatePrefabCommand::Create(target, reflection.Value(), data.Value());
    REQUIRE(command);
    auto before = SceneSerializer::Serialize(target, reflection.Value());
    REQUIRE_FALSE(command.Value()->Execute());
    REQUIRE(SceneSerializer::Serialize(target, reflection.Value()).Value() == before.Value());
}

TEST_CASE("Prefab enforces entity depth file and path limits", "[prefab]")
{
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    Scene source;
    auto entity = source.CreateEntity();
    auto id = source.GetComponent<EntityIdentityComponent>(entity)->id;
    auto data = Prefab::Capture(source, id, reflection.Value());
    REQUIRE(data);
    auto doc = nlohmann::json::parse(data.Value());
    SECTION("entity limit")
    {
        const auto record = doc["scene"]["entities"][0];
        for (usize index = 1; index < Prefab::MaxEntities; ++index)
        {
            auto child = record;
            child["id"] = UUID::Random().ToString();
            child["parent"] = id.ToString();
            child["siblingOrder"] = index - 1;
            doc["scene"]["entities"].push_back(child);
        }
        REQUIRE(Prefab::Parse(doc.dump(), reflection.Value()));
        doc["scene"]["entities"].push_back(record);
        REQUIRE_FALSE(Prefab::Parse(doc.dump(), reflection.Value()));
    }
    SECTION("depth limit on disk")
    {
        auto record = doc["scene"]["entities"][0];
        for (usize index = 1; index < Prefab::MaxDepth; ++index)
        {
            record["parent"] = record["id"];
            record["id"] = UUID::Random().ToString();
            doc["scene"]["entities"].push_back(record);
        }
        REQUIRE(Prefab::Parse(doc.dump(), reflection.Value()));
        record["parent"] = record["id"];
        record["id"] = UUID::Random().ToString();
        doc["scene"]["entities"].push_back(record);
        REQUIRE_FALSE(Prefab::Parse(doc.dump(), reflection.Value()));
    }
    SECTION("bounded disk load and asset type")
    {
        Test::AssetTempDirectory temp;
        AssetRegistry assets;
        auto handle = assets.Register(AssetType::Prefab, "Robot.prefab");
        REQUIRE(handle);
        REQUIRE_FALSE(
            Prefab::LoadRegistered(assets, temp.Path(), handle.Value(), reflection.Value()));
        REQUIRE(FileSystem::WriteText(temp.Path() / "Robot.prefab", data.Value()));
        REQUIRE(Prefab::LoadRegistered(assets, temp.Path(), handle.Value(), reflection.Value()));
        auto wrong = assets.Register(AssetType::Texture, "Other.prefab");
        REQUIRE(wrong);
        REQUIRE_FALSE(
            Prefab::LoadRegistered(assets, temp.Path(), wrong.Value(), reflection.Value()));
        doc["scene"]["entities"][0]["components"]["LuaScript"] = {
            {"script", UUID::Random().ToString()}, {"enabled", true}};
        REQUIRE(FileSystem::WriteText(temp.Path() / "Robot.prefab", doc.dump()));
        REQUIRE_FALSE(
            Prefab::LoadRegistered(assets, temp.Path(), handle.Value(), reflection.Value()));
        doc["scene"]["entities"][0]["components"]["LuaScript"]["script"] = wrong.Value().ToString();
        REQUIRE(FileSystem::WriteText(temp.Path() / "Robot.prefab", doc.dump()));
        REQUIRE_FALSE(
            Prefab::LoadRegistered(assets, temp.Path(), handle.Value(), reflection.Value()));
        REQUIRE(FileSystem::WriteText(temp.Path() / "Robot.prefab",
                                      std::string(Prefab::MaxBytes + 1, ' ')));
        REQUIRE_FALSE(
            Prefab::LoadRegistered(assets, temp.Path(), handle.Value(), reflection.Value()));
        REQUIRE_FALSE(assets.Register(AssetType::Prefab, "../escape.prefab"));
    }
    SECTION("nested JSON")
    {
        REQUIRE_FALSE(
            Prefab::Parse(std::string(40, '[') + std::string(40, ']'), reflection.Value()));
    }
}
