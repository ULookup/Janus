#include "Scene/Command/EntityCommands.h"

#include "Core/Reflection/ReflectionRegistry.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "UI/UIComponents.h"

#include <algorithm>
#include <cstddef>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Janus
{
namespace
{

Result<ECS::Entity> ResolveEntity(
    Scene& scene,
    UUID id)
{
    if (!id.IsValid())
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::InvalidArgument,
            "Entity command requires a valid persistent UUID.");
    }

    const ECS::Entity entity =
        scene.FindEntity(id);
    if (!entity.IsValid())
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::EntityNotFound,
            "Entity command target no longer exists.");
    }

    return Result<ECS::Entity>::Success(entity);
}

usize SiblingOrder(
    const Scene& scene,
    ECS::Entity entity,
    const HierarchyComponent& hierarchy)
{
    if (!hierarchy.parent.IsValid())
    {
        return 0;
    }

    const auto* parentHierarchy =
        scene.GetComponent<HierarchyComponent>(
            hierarchy.parent);
    if (parentHierarchy == nullptr)
    {
        return 0;
    }

    usize order = 0;
    ECS::Entity child =
        parentHierarchy->firstChild;

    while (child.IsValid())
    {
        if (child == entity)
        {
            return order;
        }

        const auto* childHierarchy =
            scene.GetComponent<HierarchyComponent>(
                child);
        if (childHierarchy == nullptr)
        {
            break;
        }

        child = childHierarchy->nextSibling;
        ++order;
    }

    return order;
}

Result<ReflectedComponentSnapshot>
CaptureComponent(
    Scene& scene,
    const SceneReflection& reflection,
    UUID entity,
    const ComponentDescriptor& descriptor)
{
    ReflectedComponentSnapshot snapshot;
    snapshot.component = descriptor.id;

    for (const PropertyDescriptor& property :
         descriptor.properties)
    {
        if (!property.serializable)
        {
            continue;
        }

        auto value = reflection.GetProperty(
            scene,
            entity,
            descriptor.id,
            property.id);
        if (!value)
        {
            return Result<ReflectedComponentSnapshot>::Failure(
                value.GetError());
        }

        snapshot.properties.push_back(
            ReflectedPropertySnapshot{
                property.id,
                std::move(value).Value()});
    }

    return Result<ReflectedComponentSnapshot>::Success(
        std::move(snapshot));
}

Result<std::vector<ReflectedComponentSnapshot>>
CaptureComponents(
    Scene& scene,
    const SceneReflection& reflection,
    UUID entity)
{
    std::vector<ReflectedComponentSnapshot> snapshots;

    for (const ComponentDescriptor* descriptor :
         reflection.GetRegistry().GetComponents())
    {
        if (descriptor == nullptr
            || !descriptor->serializable)
        {
            continue;
        }

        auto present = reflection.HasComponent(
            scene,
            entity,
            descriptor->id);
        if (!present)
        {
            return Result<std::vector<ReflectedComponentSnapshot>>::Failure(
                present.GetError());
        }

        if (!present.Value())
        {
            continue;
        }

        auto snapshot = CaptureComponent(
            scene,
            reflection,
            entity,
            *descriptor);
        if (!snapshot)
        {
            return Result<std::vector<ReflectedComponentSnapshot>>::Failure(
                snapshot.GetError());
        }

        snapshots.push_back(
            std::move(snapshot).Value());
    }

    return Result<std::vector<ReflectedComponentSnapshot>>::Success(
        std::move(snapshots));
}

Result<void> CaptureEntityRecursive(
    Scene& scene,
    const SceneReflection& reflection,
    ECS::Entity entity,
    EntitySubtreeSnapshot& snapshot)
{
    const auto* identity =
        scene.GetComponent<EntityIdentityComponent>(
            entity);
    const auto* hierarchy =
        scene.GetComponent<HierarchyComponent>(
            entity);

    if (identity == nullptr || hierarchy == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Deleted entity is missing persistent identity or hierarchy state.");
    }

    EntityAuthoringSnapshot record;
    record.id = identity->id;
    record.name = identity->name;
    record.siblingOrder =
        SiblingOrder(scene, entity, *hierarchy);

    if (hierarchy->parent.IsValid())
    {
        const auto* parentIdentity =
            scene.GetComponent<EntityIdentityComponent>(
                hierarchy->parent);
        if (parentIdentity == nullptr)
        {
            return Result<void>::Failure(
                ErrorCode::InvalidState,
                "Deleted entity parent is missing persistent identity.");
        }

        record.parent = parentIdentity->id;
    }

    auto components =
        CaptureComponents(
            scene,
            reflection,
            identity->id);
    if (!components)
    {
        return Result<void>::Failure(
            components.GetError());
    }

    record.components =
        std::move(components).Value();
    snapshot.entities.push_back(
        std::move(record));

    ECS::Entity child =
        hierarchy->firstChild;
    while (child.IsValid())
    {
        const auto* childHierarchy =
            scene.GetComponent<HierarchyComponent>(
                child);
        if (childHierarchy == nullptr)
        {
            return Result<void>::Failure(
                ErrorCode::InvalidState,
                "Deleted entity subtree contains invalid hierarchy links.");
        }

        const ECS::Entity next =
            childHierarchy->nextSibling;

        auto captured =
            CaptureEntityRecursive(
                scene,
                reflection,
                child,
                snapshot);
        if (!captured)
        {
            return captured;
        }

        child = next;
    }

    return Result<void>::Success();
}

bool SnapshotContains(
    const EntitySubtreeSnapshot& snapshot,
    UUID id)
{
    return std::any_of(
        snapshot.entities.begin(),
        snapshot.entities.end(),
        [id](const EntityAuthoringSnapshot& entity)
        {
            return entity.id == id;
        });
}

void CleanupSnapshotEntities(
    Scene& scene,
    const EntitySubtreeSnapshot& snapshot) noexcept
{
    const ECS::Entity root =
        scene.FindEntity(snapshot.root);
    if (root.IsValid())
    {
        scene.DestroyEntity(root);
    }

    for (const EntityAuthoringSnapshot& record :
         snapshot.entities)
    {
        const ECS::Entity entity =
            scene.FindEntity(record.id);
        if (entity.IsValid())
        {
            scene.DestroyEntity(entity);
        }
    }
}

Result<void> RestoreComponents(
    Scene& scene,
    const SceneReflection& reflection,
    const EntityAuthoringSnapshot& record)
{
    for (const ReflectedComponentSnapshot& component :
         record.components)
    {
        auto present = reflection.HasComponent(
            scene,
            record.id,
            component.component);
        if (!present)
        {
            return Result<void>::Failure(
                present.GetError());
        }

        if (!present.Value())
        {
            auto added = reflection.AddComponent(
                scene,
                record.id,
                component.component);
            if (!added)
            {
                return added;
            }
        }

        for (const ReflectedPropertySnapshot& property :
             component.properties)
        {
            auto restored =
                reflection.RestoreProperty(
                    scene,
                    record.id,
                    component.component,
                    property.property,
                    property.value);
            if (!restored)
            {
                return restored;
            }
        }

        auto valid = reflection.ValidateComponent(
            scene,
            record.id,
            component.component);
        if (!valid)
        {
            return valid;
        }
    }

    return Result<void>::Success();
}

Result<std::vector<ECS::Entity>> CurrentChildren(
    Scene& scene,
    ECS::Entity parent)
{
    const auto* hierarchy =
        scene.GetComponent<HierarchyComponent>(
            parent);
    if (hierarchy == nullptr)
    {
        return Result<std::vector<ECS::Entity>>::Failure(
            ErrorCode::InvalidState,
            "External parent is missing hierarchy state.");
    }

    std::vector<ECS::Entity> children;
    ECS::Entity child =
        hierarchy->firstChild;

    while (child.IsValid())
    {
        children.push_back(child);

        const auto* childHierarchy =
            scene.GetComponent<HierarchyComponent>(
                child);
        if (childHierarchy == nullptr)
        {
            return Result<std::vector<ECS::Entity>>::Failure(
                ErrorCode::InvalidState,
                "External parent contains invalid child hierarchy state.");
        }

        child = childHierarchy->nextSibling;
    }

    return Result<std::vector<ECS::Entity>>::Success(
        std::move(children));
}

Result<void> ReorderChildren(
    Scene& scene,
    ECS::Entity parent,
    const std::vector<ECS::Entity>& desired)
{
    for (const ECS::Entity child : desired)
    {
        auto detached =
            scene.SetParent(
                child,
                ECS::Entity{});
        if (!detached)
        {
            return detached;
        }
    }

    for (auto it = desired.rbegin();
         it != desired.rend();
         ++it)
    {
        auto attached =
            scene.SetParent(
                *it,
                parent);
        if (!attached)
        {
            return attached;
        }
    }

    return Result<void>::Success();
}

} // namespace

ReparentEntityCommand::ReparentEntityCommand(Scene& scene, UUID entity, UUID parent, usize index)
    : m_Scene(scene), m_Entity(entity), m_Parent(parent), m_Index(index)
{
}

Result<void> ReparentEntityCommand::Apply(UUID parentId, usize index)
{
    auto child = ResolveEntity(m_Scene, m_Entity);
    if (!child)
        return Result<void>::Failure(child.GetError());
    ECS::Entity parent;
    if (parentId.IsValid())
    {
        auto resolved = ResolveEntity(m_Scene, parentId);
        if (!resolved)
            return Result<void>::Failure(resolved.GetError());
        parent = resolved.Value();
    }
    if (m_Scene.HasComponent<CanvasComponent>(child.Value()) && parent.IsValid())
        return Result<void>::Failure(ErrorCode::InvalidArgument,
                                     "Canvas must remain a root entity.");
    auto canvasOf = [&](ECS::Entity entity)
    {
        while (entity.IsValid())
        {
            if (m_Scene.HasComponent<CanvasComponent>(entity))
                return entity;
            entity = m_Scene.GetComponent<HierarchyComponent>(entity)->parent;
        }
        return ECS::Entity{};
    };
    const auto oldCanvas = canvasOf(child.Value());
    const auto newCanvas = canvasOf(parent);
    if (oldCanvas.IsValid() && newCanvas.IsValid() && oldCanvas != newCanvas)
        return Result<void>::Failure(ErrorCode::InvalidArgument,
                                     "Cross-Canvas reparent is unsupported.");
    std::vector<ECS::Entity> desired;
    if (parent.IsValid())
    {
        auto children = CurrentChildren(m_Scene, parent);
        if (!children)
            return Result<void>::Failure(children.GetError());
        desired = std::move(children).Value();
        std::erase(desired, child.Value());
        if (index > desired.size())
            return Result<void>::Failure(ErrorCode::InvalidArgument,
                                         "Sibling index is outside the target child list.");
        desired.insert(desired.begin() + static_cast<std::ptrdiff_t>(index), child.Value());
    }
    else if (index != 0)
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Root order is persistent UUID order; root sibling index must be zero.");
    auto attached = m_Scene.SetParent(child.Value(), parent);
    if (!attached)
        return attached;
    // SetParent validates cycles before touching links. All members of desired
    // are now valid direct children, so restoring order cannot introduce a cycle.
    if (parent.IsValid())
        return ReorderChildren(m_Scene, parent, desired);
    return Result<void>::Success();
}
Result<void> ReparentEntityCommand::Execute()
{
    if (m_Captured)
        return Result<void>::Failure(ErrorCode::InvalidState, "Reparent has already executed.");
    auto child = ResolveEntity(m_Scene, m_Entity);
    if (!child)
        return Result<void>::Failure(child.GetError());
    const auto& hierarchy = *m_Scene.GetComponent<HierarchyComponent>(child.Value());
    m_OldParent = hierarchy.parent.IsValid()
                      ? m_Scene.GetComponent<EntityIdentityComponent>(hierarchy.parent)->id
                      : UUID{};
    m_OldIndex = SiblingOrder(m_Scene, child.Value(), hierarchy);
    auto result = Apply(m_Parent, m_Index);
    if (result)
        m_Captured = true;
    return result;
}
Result<void> ReparentEntityCommand::Undo()
{
    if (!m_Captured)
        return Result<void>::Failure(ErrorCode::InvalidState, "Reparent has no snapshot.");
    return Apply(m_OldParent, m_OldIndex);
}
Result<void> ReparentEntityCommand::Redo()
{
    if (!m_Captured)
        return Result<void>::Failure(ErrorCode::InvalidState, "Reparent has no snapshot.");
    return Apply(m_Parent, m_Index);
}
std::string_view ReparentEntityCommand::Describe() const noexcept
{
    return "Reparent Entity";
}
Result<usize> ReparentEntityCommand::EstimateUndoBytes() const
{
    return Result<usize>::Success(sizeof(*this));
}
std::vector<CommandEffect> ReparentEntityCommand::GetEffects() const
{
    return {{m_Entity, "ReparentEntity"}};
}

CreateEntityCommand::CreateEntityCommand(
    Scene& scene,
    UUID entity,
    std::string name)
    : m_Scene(scene),
      m_Entity(entity),
      m_Name(std::move(name))
{
}

Result<void> CreateEntityCommand::Create()
{
    if (!m_Entity.IsValid())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "CreateEntityCommand requires a valid UUID.");
    }

    if (m_Name.empty())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "CreateEntityCommand requires a non-empty name.");
    }

    auto created =
        m_Scene.CreateEntityWithUUID(
            m_Entity,
            m_Name);
    if (!created)
    {
        return Result<void>::Failure(
            created.GetError());
    }

    return Result<void>::Success();
}

Result<void> CreateEntityCommand::Execute()
{
    return Create();
}

Result<void> CreateEntityCommand::Undo()
{
    const ECS::Entity entity =
        m_Scene.FindEntity(m_Entity);
    if (!entity.IsValid())
    {
        return Result<void>::Failure(
            ErrorCode::EntityNotFound,
            "Created entity no longer exists for Undo.");
    }

    if (!m_Scene.DestroyEntity(entity))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidEntity,
            "Failed to destroy created entity during Undo.");
    }

    return Result<void>::Success();
}

Result<void> CreateEntityCommand::Redo()
{
    return Create();
}

std::string_view CreateEntityCommand::Describe() const noexcept
{
    return "Create Entity";
}

RenameEntityCommand::RenameEntityCommand(
    Scene& scene,
    UUID entity,
    std::string name)
    : m_Scene(scene),
      m_Entity(entity),
      m_NewName(std::move(name))
{
}

Result<void> RenameEntityCommand::ApplyName(
    const std::string& name)
{
    if (name.empty())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Entity name cannot be empty.");
    }

    auto entity = ResolveEntity(
        m_Scene,
        m_Entity);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    auto* identity =
        m_Scene.GetComponent<EntityIdentityComponent>(
            entity.Value());
    if (identity == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity is missing persistent identity.");
    }

    identity->name = name;
    return Result<void>::Success();
}

Result<void> RenameEntityCommand::Execute()
{
    if (m_OldName.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "RenameEntityCommand may only execute once before history owns it.");
    }

    auto entity = ResolveEntity(
        m_Scene,
        m_Entity);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    const auto* identity =
        m_Scene.GetComponent<EntityIdentityComponent>(
            entity.Value());
    if (identity == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity is missing persistent identity.");
    }

    m_OldName = identity->name;
    auto applied = ApplyName(m_NewName);
    if (!applied)
    {
        m_OldName.reset();
        return applied;
    }

    return Result<void>::Success();
}

Result<void> RenameEntityCommand::Undo()
{
    if (!m_OldName.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "RenameEntityCommand has no captured original name.");
    }

    return ApplyName(*m_OldName);
}

Result<void> RenameEntityCommand::Redo()
{
    if (!m_OldName.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "RenameEntityCommand has no captured original name.");
    }

    return ApplyName(m_NewName);
}

std::string_view RenameEntityCommand::Describe() const noexcept
{
    return "Rename Entity";
}

DeleteEntityCommand::DeleteEntityCommand(
    Scene& scene,
    SceneReflection reflection,
    UUID entity)
    : m_Scene(scene),
      m_Reflection(std::move(reflection)),
      m_Entity(entity)
{
}

Result<EntitySubtreeSnapshot> CaptureEntitySubtree(Scene& scene, const SceneReflection& reflection,
                                                   UUID id)
{
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<EntitySubtreeSnapshot>::Failure(
            entity.GetError());
    }

    EntitySubtreeSnapshot snapshot;
    snapshot.root = id;

    auto captured = CaptureEntityRecursive(scene, reflection, entity.Value(), snapshot);
    if (!captured)
    {
        return Result<EntitySubtreeSnapshot>::Failure(
            captured.GetError());
    }

    return Result<EntitySubtreeSnapshot>::Success(
        std::move(snapshot));
}

Result<EntitySubtreeSnapshot> DeleteEntityCommand::CaptureSnapshot() const
{
    return CaptureEntitySubtree(m_Scene, m_Reflection, m_Entity);
}

Result<void> DeleteEntityCommand::Execute()
{
    if (m_Snapshot.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "DeleteEntityCommand may only execute once before history owns it.");
    }

    auto snapshot = CaptureSnapshot();
    if (!snapshot)
    {
        return Result<void>::Failure(
            snapshot.GetError());
    }

    const ECS::Entity entity =
        m_Scene.FindEntity(m_Entity);
    if (!entity.IsValid()
        || !m_Scene.DestroyEntity(entity))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidEntity,
            "Failed to destroy entity subtree.");
    }

    m_Snapshot =
        std::move(snapshot).Value();
    return Result<void>::Success();
}

Result<void> RestoreEntitySubtree(Scene& scene, const SceneReflection& reflection,
                                  const EntitySubtreeSnapshot& snapshot)
{
    for (const EntityAuthoringSnapshot& record :
         snapshot.entities)
    {
        if (scene.FindEntity(record.id).IsValid())
        {
            return Result<void>::Failure(
                ErrorCode::InvalidState,
                "Cannot restore deleted subtree because UUID "
                    + record.id.ToString()
                    + " already exists.");
        }
    }

    for (const EntityAuthoringSnapshot& record :
         snapshot.entities)
    {
        auto created = scene.CreateEntityWithUUID(record.id, record.name);
        if (!created)
        {
            CleanupSnapshotEntities(scene, snapshot);
            return Result<void>::Failure(
                created.GetError());
        }
    }

    for (const EntityAuthoringSnapshot& record :
         snapshot.entities)
    {
        auto restored = RestoreComponents(scene, reflection, record);
        if (!restored)
        {
            CleanupSnapshotEntities(scene, snapshot);
            return restored;
        }
    }

    struct PendingParent
    {
        UUID child;
        UUID parent;
        usize order = 0;
    };

    std::vector<PendingParent> pending;

    for (const EntityAuthoringSnapshot& record :
         snapshot.entities)
    {
        if (record.parent.has_value()
            && SnapshotContains(
                snapshot,
                *record.parent))
        {
            pending.push_back(
                PendingParent{
                    record.id,
                    *record.parent,
                    record.siblingOrder});
        }
    }

    std::sort(
        pending.begin(),
        pending.end(),
        [](const PendingParent& left,
           const PendingParent& right)
        {
            if (left.parent != right.parent)
            {
                return left.parent < right.parent;
            }
            if (left.order != right.order)
            {
                return left.order > right.order;
            }
            return left.child > right.child;
        });

    for (const PendingParent& relation :
         pending)
    {
        const ECS::Entity child = scene.FindEntity(relation.child);
        const ECS::Entity parent = scene.FindEntity(relation.parent);

        if (!child.IsValid()
            || !parent.IsValid())
        {
            CleanupSnapshotEntities(scene, snapshot);
            return Result<void>::Failure(
                ErrorCode::InvalidState,
                "Failed to resolve restored internal hierarchy.");
        }

        auto parented = scene.SetParent(child, parent);
        if (!parented)
        {
            CleanupSnapshotEntities(scene, snapshot);
            return parented;
        }
    }

    const auto rootRecord =
        std::find_if(
            snapshot.entities.begin(),
            snapshot.entities.end(),
            [&](const EntityAuthoringSnapshot& record)
            {
                return record.id == snapshot.root;
            });

    if (rootRecord == snapshot.entities.end())
    {
        CleanupSnapshotEntities(scene, snapshot);
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Deleted subtree snapshot is missing its root record.");
    }

    if (rootRecord->parent.has_value()
        && !SnapshotContains(
            snapshot,
            *rootRecord->parent))
    {
        const ECS::Entity externalParent = scene.FindEntity(*rootRecord->parent);

        if (externalParent.IsValid())
        {
            auto children = CurrentChildren(scene, externalParent);
            if (!children)
            {
                CleanupSnapshotEntities(scene, snapshot);
                return Result<void>::Failure(
                    children.GetError());
            }

            const ECS::Entity restoredRoot = scene.FindEntity(snapshot.root);
            if (!restoredRoot.IsValid())
            {
                CleanupSnapshotEntities(scene, snapshot);
                return Result<void>::Failure(
                    ErrorCode::InvalidState,
                    "Restored subtree root is missing.");
            }

            auto desired =
                std::move(children).Value();

            const usize insertAt =
                std::min(
                    rootRecord->siblingOrder,
                    desired.size());

            desired.insert(
                desired.begin()
                    + static_cast<std::ptrdiff_t>(insertAt),
                restoredRoot);

            auto reordered = ReorderChildren(scene, externalParent, desired);
            if (!reordered)
            {
                CleanupSnapshotEntities(scene, snapshot);
                return reordered;
            }
        }
    }

    return Result<void>::Success();
}

Result<void> DeleteEntityCommand::RestoreSnapshot()
{
    if (!m_Snapshot)
        return Result<void>::Failure(ErrorCode::InvalidState, "Delete has no captured snapshot.");
    return RestoreEntitySubtree(m_Scene, m_Reflection, *m_Snapshot);
}

void DeleteEntityCommand::CleanupRestoredEntities() noexcept
{
    if (!m_Snapshot.has_value())
    {
        return;
    }

    CleanupSnapshotEntities(
        m_Scene,
        *m_Snapshot);
}

Result<void> DeleteEntityCommand::Undo()
{
    return RestoreSnapshot();
}

Result<void> DeleteEntityCommand::Redo()
{
    if (!m_Snapshot.has_value())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "DeleteEntityCommand has no captured subtree snapshot.");
    }

    const ECS::Entity entity =
        m_Scene.FindEntity(m_Entity);
    if (!entity.IsValid())
    {
        return Result<void>::Failure(
            ErrorCode::EntityNotFound,
            "Deleted subtree root no longer exists for Redo.");
    }

    if (!m_Scene.DestroyEntity(entity))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidEntity,
            "Failed to destroy restored entity subtree during Redo.");
    }

    return Result<void>::Success();
}

std::string_view DeleteEntityCommand::Describe() const noexcept
{
    return "Delete Entity";
}

Result<usize> CreateEntityCommand::EstimateUndoBytes() const
{
    return Result<usize>::Success(512 + m_Name.size());
}
Result<usize> RenameEntityCommand::EstimateUndoBytes() const
{
    const auto* identity =
        m_Scene.GetComponent<EntityIdentityComponent>(m_Scene.FindEntity(m_Entity));
    return Result<usize>::Success(512 + m_NewName.size() + (identity ? identity->name.size() : 0));
}
Result<usize> DeleteEntityCommand::EstimateUndoBytes() const
{
    return EstimateSceneCommandUndoBytes(m_Scene, m_Reflection);
}
std::vector<CommandEffect> CreateEntityCommand::GetEffects() const
{
    return {{m_Entity, "CreateEntity"}};
}
std::vector<CommandEffect> RenameEntityCommand::GetEffects() const
{
    return {{m_Entity, "RenameEntity"}};
}
std::vector<CommandEffect> DeleteEntityCommand::GetEffects() const
{
    std::vector<CommandEffect> effects;
    if (m_Snapshot)
        for (const auto& entity : m_Snapshot->entities)
            effects.push_back({entity.id, "DeleteEntity"});
    else
        effects.push_back({m_Entity, "DeleteEntity"});
    return effects;
}

} // namespace Janus
