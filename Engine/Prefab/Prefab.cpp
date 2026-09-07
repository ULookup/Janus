#include "Prefab/Prefab.h"

#include "Core/Reflection/ReflectionRegistry.h"
#include "Project/ProjectSettings.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneSerializer.h"
#include "UI/UIComponents.h"

#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>

namespace Janus
{
namespace
{
using Json = nlohmann::json;
Result<EntitySubtreeSnapshot> Invalid(std::string message)
{
    return Result<EntitySubtreeSnapshot>::Failure(ErrorCode::InvalidArgument, std::move(message));
}

// Reject excessive nesting before the JSON parser allocates a recursive document.
bool BoundedJson(std::string_view text)
{
    int depth = 0;
    bool quoted = false, escaped = false;
    for (char c : text)
    {
        if (quoted)
        {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
        }
        else if (c == '"')
            quoted = true;
        else if (c == '[' || c == '{')
        {
            if (++depth > 32)
                return false;
        }
        else if (c == ']' || c == '}')
        {
            if (--depth < 0)
                return false;
        }
    }
    return depth == 0 && !quoted;
}

Result<void> CheckSubtree(Scene& scene, UUID root)
{
    std::vector<std::pair<ECS::Entity, usize>> pending{{scene.FindEntity(root), 1}};
    std::set<UUID> visited;
    while (!pending.empty())
    {
        auto [entity, depth] = pending.back();
        pending.pop_back();
        const auto* identity = scene.GetComponent<EntityIdentityComponent>(entity);
        const auto* hierarchy = scene.GetComponent<HierarchyComponent>(entity);
        if (!identity || !hierarchy || depth > Prefab::MaxDepth ||
            !visited.insert(identity->id).second || visited.size() > Prefab::MaxEntities)
            return Result<void>::Failure(ErrorCode::InvalidArgument,
                                         "Invalid or oversized Prefab subtree.");
        auto child = hierarchy->firstChild;
        usize siblings = 0;
        while (child.IsValid())
        {
            const auto* links = scene.GetComponent<HierarchyComponent>(child);
            if (!links || links->parent != entity || ++siblings > Prefab::MaxEntities)
                return Result<void>::Failure(ErrorCode::InvalidArgument,
                                             "Invalid Prefab child links.");
            pending.emplace_back(child, depth + 1);
            child = links->nextSibling;
        }
    }
    return Result<void>::Success();
}
} // namespace

Result<std::string> Prefab::Capture(Scene& scene, UUID root, const ReflectionRegistry& reflection)
{
    auto valid = CheckSubtree(scene, root);
    if (!valid)
        return Result<std::string>::Failure(valid.GetError());
    auto snapshot = CaptureEntitySubtree(scene, SceneReflection(reflection), root);
    if (!snapshot)
        return Result<std::string>::Failure(snapshot.GetError());
    usize stringBytes = 0;
    auto validString = [&](const std::string& value)
    {
        if (value.size() > MaxBytes - stringBytes)
            return false;
        stringBytes += value.size();
        // Replacement mode makes invalid native-authored UTF-8 observable without throwing.
        auto encoded = Json(value).dump(-1, ' ', false, Json::error_handler_t::replace);
        return Json::parse(encoded).get<std::string>() == value;
    };
    for (const auto& entity : snapshot.Value().entities)
    {
        if (!validString(entity.name))
            return Result<std::string>::Failure(
                ErrorCode::InvalidArgument, "Prefab names exceed bounds or contain invalid UTF-8.");
        for (const auto& component : entity.components)
            for (const auto& property : component.properties)
                if (const auto* value = std::get_if<std::string>(&property.value);
                    value && !validString(*value))
                    return Result<std::string>::Failure(
                        ErrorCode::InvalidArgument,
                        "Prefab strings exceed bounds or contain invalid UTF-8.");
    }
    snapshot.Value().entities.front().parent.reset();
    snapshot.Value().entities.front().siblingOrder = 0;
    Scene scratch;
    auto restored = RestoreEntitySubtree(scratch, SceneReflection(reflection), snapshot.Value());
    if (!restored)
        return Result<std::string>::Failure(restored.GetError());
    auto serialized = SceneSerializer::Serialize(scratch, reflection);
    if (!serialized)
        return serialized;
    Json document{{"schema", "janus.prefab"},
                  {"version", 1},
                  {"root", root.ToString()},
                  {"scene", Json::parse(serialized.Value())}};
    auto text = document.dump(2) + "\n";
    auto parsed = Parse(text, reflection);
    if (!parsed)
        return Result<std::string>::Failure(parsed.GetError());
    return Result<std::string>::Success(std::move(text));
}

Result<EntitySubtreeSnapshot> Prefab::Parse(std::string_view text,
                                            const ReflectionRegistry& reflection)
{
    if (text.size() > MaxBytes || !BoundedJson(text))
        return Invalid("Prefab exceeds JSON bounds.");
    bool duplicate = false;
    std::vector<std::set<std::string>> keys;
    auto callback = [&](int, Json::parse_event_t event, Json& value)
    {
        if (event == Json::parse_event_t::object_start)
            keys.emplace_back();
        if (event == Json::parse_event_t::key &&
            !keys.back().insert(value.get<std::string>()).second)
            duplicate = true;
        if (event == Json::parse_event_t::object_end)
            keys.pop_back();
        return true;
    };
    auto doc = Json::parse(text, callback, false);
    if (doc.is_discarded() || duplicate || !doc.is_object() || doc.size() != 4 ||
        !doc.contains("schema") || doc["schema"] != "janus.prefab" || !doc.contains("version") ||
        !doc["version"].is_number_integer() || doc["version"] != 1 || !doc.contains("root") ||
        !doc["root"].is_string() || !doc.contains("scene"))
        return Invalid("Expected janus.prefab version 1 with root and scene.");
    auto root = UUID::Parse(doc["root"].get<std::string>());
    if (!root || !root.Value().IsValid())
        return Invalid("Invalid Prefab root UUID.");
    const auto& scene = doc["scene"];
    if (!scene.is_object() || scene.size() != 4 || !scene.contains("scene") ||
        !scene["scene"].is_object() || scene["scene"].size() != 2 || !scene.contains("entities") ||
        !scene["entities"].is_array() || scene["entities"].empty() ||
        scene["entities"].size() > MaxEntities)
        return Invalid("Prefab requires 1..1024 entities.");
    std::map<UUID, UUID> parents;
    std::map<UUID, std::set<usize>> orders;
    for (const auto& record : scene["entities"])
    {
        if (!record.is_object() || record.size() != 5 || !record.contains("id") ||
            !record["id"].is_string() || !record.contains("parent") ||
            !record.contains("siblingOrder") || !record["siblingOrder"].is_number_unsigned() ||
            record["siblingOrder"] >= MaxEntities)
            return Invalid("Invalid Prefab entity structure or sibling order.");
        auto id = UUID::Parse(record["id"].get<std::string>());
        if (!id || !id.Value().IsValid())
            return Invalid("Invalid Prefab entity UUID.");
        UUID parent;
        if (!record["parent"].is_null())
        {
            if (!record["parent"].is_string())
                return Invalid("Invalid Prefab parent.");
            auto parsed = UUID::Parse(record["parent"].get<std::string>());
            if (!parsed || !parsed.Value().IsValid())
                return Invalid("Invalid Prefab parent UUID.");
            parent = parsed.Value();
        }
        if (!parents.emplace(id.Value(), parent).second)
            return Invalid("Duplicate Prefab entity UUID.");
        const auto order = record["siblingOrder"].get<usize>();
        if (!orders[parent].insert(order).second)
            return Invalid("Duplicate Prefab sibling order.");
        if (!parent.IsValid() && (id.Value() != root.Value() || order != 0))
            return Invalid("Prefab must contain exactly its declared root.");
    }
    if (!parents.contains(root.Value()) || parents.at(root.Value()).IsValid())
        return Invalid("Prefab root is missing or parented.");
    for (const auto& [parent, siblings] : orders)
    {
        (void)parent;
        if (*siblings.rbegin() != siblings.size() - 1)
            return Invalid("Prefab sibling order has gaps.");
    }
    // Validate the complete graph before Scene allocates entities or follows hierarchy links.
    for (const auto& [id, parent] : parents)
    {
        (void)parent;
        UUID cursor = id;
        usize depth = 0;
        while (cursor.IsValid())
        {
            auto it = parents.find(cursor);
            if (it == parents.end() || ++depth > MaxDepth)
                return Invalid("Prefab hierarchy is broken, cyclic or deeper than 64 levels.");
            cursor = it->second;
        }
    }
    auto decoded = SceneDeserializer::Deserialize(scene.dump(), reflection);
    if (!decoded)
        return Result<EntitySubtreeSnapshot>::Failure(decoded.GetError());
    return CaptureEntitySubtree(*decoded.Value(), SceneReflection(reflection), root.Value());
}

Result<std::string> Prefab::LoadRegistered(const AssetRegistry& assets,
                                           const std::filesystem::path& projectRoot,
                                           AssetHandle asset, const ReflectionRegistry& reflection)
{
    const auto* metadata = assets.Find(asset);
    if (!metadata || metadata->type != AssetType::Prefab)
        return Result<std::string>::Failure(ErrorCode::InvalidArgument,
                                            "Expected a registered Prefab asset.");
    auto path = ResolveProjectPath(projectRoot, metadata->relativePath);
    if (!path)
        return Result<std::string>::Failure(path.GetError());
    std::ifstream stream(path.Value(), std::ios::binary);
    if (!stream)
        return Result<std::string>::Failure(ErrorCode::InvalidArgument,
                                            "Cannot open Prefab asset.");
    std::string text(MaxBytes + 1, '\0');
    stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (stream.bad())
        return Result<std::string>::Failure(ErrorCode::InvalidArgument,
                                            "Cannot read Prefab asset.");
    text.resize(static_cast<usize>(stream.gcount()));
    auto parsed = Parse(text, reflection);
    if (!parsed)
        return Result<std::string>::Failure(parsed.GetError());
    for (const auto& entity : parsed.Value().entities)
        for (const auto& component : entity.components)
            for (const auto& property : component.properties)
            {
                const auto* reference = std::get_if<AssetReferenceValue>(&property.value);
                if (!reference || !reference->id.IsValid())
                    continue;
                const auto* assetMetadata = assets.Find(AssetHandle{reference->id});
                const auto* descriptor =
                    reflection.FindComponent(component.component)->FindProperty(property.property);
                if (!assetMetadata ||
                    (!descriptor->referenceConstraint.empty() &&
                     AssetTypeName(assetMetadata->type) != descriptor->referenceConstraint))
                    return Result<std::string>::Failure(
                        ErrorCode::InvalidArgument,
                        "Prefab references an unregistered asset or an incompatible asset type: " +
                            reference->id.ToString());
            }
    return Result<std::string>::Success(std::move(text));
}

InstantiatePrefabCommand::InstantiatePrefabCommand(Scene& scene,
                                                   const ReflectionRegistry& reflection,
                                                   EntitySubtreeSnapshot snapshot)
    : m_Scene(scene), m_Reflection(reflection), m_Snapshot(std::move(snapshot))
{
}

Result<std::unique_ptr<InstantiatePrefabCommand>>
InstantiatePrefabCommand::Create(Scene& scene, const ReflectionRegistry& reflection,
                                 std::string_view text)
{
    auto parsed = Prefab::Parse(text, reflection);
    if (!parsed)
        return Result<std::unique_ptr<InstantiatePrefabCommand>>::Failure(parsed.GetError());
    auto snapshot = std::move(parsed).Value();
    std::map<UUID, UUID> ids;
    std::set<UUID> reserved;
    for (const auto& entity : snapshot.entities)
        reserved.insert(entity.id);
    for (const auto& entity : snapshot.entities)
    {
        UUID id;
        do
        {
            id = UUID::Random();
        } while (scene.FindEntity(id).IsValid() || !reserved.insert(id).second);
        ids.emplace(entity.id, id);
    }
    snapshot.root = ids.at(snapshot.root);
    for (auto& entity : snapshot.entities)
    {
        entity.id = ids.at(entity.id);
        if (entity.parent)
            entity.parent = ids.at(*entity.parent);
        // AssetReferenceValue and strings are not entity references. Preserve them verbatim.
    }
    return Result<std::unique_ptr<InstantiatePrefabCommand>>::Success(
        std::unique_ptr<InstantiatePrefabCommand>(
            new InstantiatePrefabCommand(scene, reflection, std::move(snapshot))));
}

Result<void> InstantiatePrefabCommand::Restore()
{
    // A second screen Canvas is unsupported by the existing UI layout contract.
    usize canvases = 0, primaryCameras = 0;
    for (auto entity : m_Scene.GetEntities())
    {
        if (m_Scene.HasComponent<CanvasComponent>(entity))
            ++canvases;
        const auto* camera = m_Scene.GetComponent<CameraComponent>(entity);
        if (camera && camera->primary)
            ++primaryCameras;
    }
    for (const auto& entity : m_Snapshot.entities)
        for (const auto& component : entity.components)
        {
            if (component.component == SceneReflectionIds::Canvas)
            {
                if (entity.parent || ++canvases > 1)
                    return Result<void>::Failure(
                        ErrorCode::InvalidArgument,
                        "Prefab would create an unsupported Canvas hierarchy.");
            }
            if (component.component == SceneReflectionIds::Camera)
                for (const auto& property : component.properties)
                    if (property.property == SceneReflectionIds::CameraPrimary &&
                        std::get<bool>(property.value) && ++primaryCameras > 1)
                        return Result<void>::Failure(
                            ErrorCode::InvalidArgument,
                            "Prefab would create multiple primary cameras.");
        }
    auto restored = RestoreEntitySubtree(m_Scene, m_Reflection, m_Snapshot);
    if (restored)
        m_Present = true;
    return restored;
}
Result<void> InstantiatePrefabCommand::Execute()
{
    if (m_Executed)
        return Result<void>::Failure(ErrorCode::InvalidState, "Prefab command already executed.");
    auto result = Restore();
    if (result)
        m_Executed = true;
    return result;
}
Result<void> InstantiatePrefabCommand::Undo()
{
    if (!m_Present || !m_Scene.DestroyEntity(m_Scene.FindEntity(m_Snapshot.root)))
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Prefab instance is unavailable for Undo.");
    m_Present = false;
    return Result<void>::Success();
}
Result<void> InstantiatePrefabCommand::Redo()
{
    if (!m_Executed || m_Present)
        return Result<void>::Failure(ErrorCode::InvalidState, "Prefab is unavailable for Redo.");
    return Restore();
}
Result<usize> InstantiatePrefabCommand::EstimateUndoBytes() const
{
    usize bytes = sizeof(*this) + m_Snapshot.entities.capacity() * sizeof(EntityAuthoringSnapshot);
    for (const auto& entity : m_Snapshot.entities)
    {
        bytes += entity.name.capacity() +
                 entity.components.capacity() * sizeof(ReflectedComponentSnapshot);
        for (const auto& component : entity.components)
        {
            bytes += component.properties.capacity() * sizeof(ReflectedPropertySnapshot);
            for (const auto& property : component.properties)
                if (const auto* value = std::get_if<std::string>(&property.value))
                    bytes += value->capacity();
        }
    }
    return Result<usize>::Success(bytes * 2 + 1024);
}
std::vector<CommandEffect> InstantiatePrefabCommand::GetEffects() const
{
    std::vector<CommandEffect> effects;
    for (const auto& entity : m_Snapshot.entities)
        effects.push_back({entity.id, "InstantiatePrefab"});
    return effects;
}
} // namespace Janus
