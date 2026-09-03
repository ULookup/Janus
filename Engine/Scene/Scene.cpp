#include "Scene/Scene.h"

#include <algorithm>
#include <utility>

namespace Janus
{

Scene::Scene()
    : Scene(SceneMetadata{UUID::Random(), "Scene"})
{
}

Scene::Scene(SceneMetadata metadata)
    : m_Metadata(std::move(metadata))
{
    if (!m_Metadata.id.IsValid())
    {
        m_Metadata.id = UUID::Random();
    }
}

Scene::~Scene() = default;

ECS::Entity Scene::CreateEntity(std::string name)
{
    UUID id;
    do
    {
        id = UUID::Random();
    }
    while (m_EntityIndex.contains(id));

    auto result = CreateEntityWithUUID(id, std::move(name));
    return std::move(result).Value();
}

Result<ECS::Entity> Scene::CreateEntityWithUUID(
    UUID id,
    std::string name)
{
    if (!id.IsValid())
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::InvalidArgument,
            "Scene entity UUID cannot be nil.");
    }

    if (m_EntityIndex.contains(id))
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::InvalidArgument,
            "Scene already contains entity UUID " + id.ToString() + ".");
    }

    const ECS::Entity entity = m_Registry.CreateEntity();
    m_Registry.AddComponent<EntityIdentityComponent>(
        entity,
        EntityIdentityComponent{id, std::move(name)});
    m_Registry.AddComponent<TransformComponent>(
        entity,
        TransformComponent{});
    m_Registry.AddComponent<HierarchyComponent>(
        entity,
        HierarchyComponent{});

    m_EntityIndex.emplace(id, entity);

    return Result<ECS::Entity>::Success(entity);
}

bool Scene::DestroyEntity(ECS::Entity entity)
{
    if (!m_Registry.IsValid(entity))
    {
        return false;
    }

    DestroyEntityRecursive(entity);
    return true;
}

Result<void> Scene::SetParent(
    ECS::Entity child,
    ECS::Entity parent)
{
    if (!m_Registry.IsValid(child))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidEntity,
            "Cannot reparent an invalid child.");
    }

    if (parent.IsValid() && !m_Registry.IsValid(parent))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidEntity,
            "Cannot reparent to an invalid parent.");
    }

    if (!m_Registry.HasComponent<HierarchyComponent>(child)
        || (parent.IsValid()
            && !m_Registry.HasComponent<HierarchyComponent>(parent)))
    {
        return Result<void>::Failure(
            ErrorCode::EntityNotFound,
            "Scene hierarchy components are required.");
    }

    if (child == parent)
    {
        return Result<void>::Failure(
            ErrorCode::HierarchyCycle,
            "An entity cannot be its own parent.");
    }

    if (parent.IsValid() && IsAncestor(parent, child))
    {
        return Result<void>::Failure(
            ErrorCode::HierarchyCycle,
            "Cannot create a hierarchy cycle.");
    }

    DetachFromParent(child);

    auto* childHierarchy =
        m_Registry.GetComponent<HierarchyComponent>(child);
    childHierarchy->parent = parent;

    if (parent.IsValid())
    {
        auto* parentHierarchy =
            m_Registry.GetComponent<HierarchyComponent>(parent);

        childHierarchy->nextSibling =
            parentHierarchy->firstChild;

        if (parentHierarchy->firstChild.IsValid())
        {
            auto* oldFirst =
                m_Registry.GetComponent<HierarchyComponent>(
                    parentHierarchy->firstChild);
            if (oldFirst != nullptr)
            {
                oldFirst->previousSibling = child;
            }
        }

        parentHierarchy->firstChild = child;
    }

    auto* transform =
        m_Registry.GetComponent<TransformComponent>(child);
    if (transform != nullptr)
    {
        transform->dirty = true;
    }

    return Result<void>::Success();
}

ECS::Entity Scene::FindEntity(UUID id) const noexcept
{
    const auto it = m_EntityIndex.find(id);
    if (it == m_EntityIndex.end() || !m_Registry.IsValid(it->second))
    {
        return ECS::Entity{};
    }

    return it->second;
}

std::vector<ECS::Entity> Scene::GetEntities() const
{
    std::vector<ECS::Entity> entities;
    entities.reserve(m_EntityIndex.size());

    for (const auto& [id, entity] : m_EntityIndex)
    {
        (void)id;
        if (m_Registry.IsValid(entity))
        {
            entities.push_back(entity);
        }
    }

    std::sort(
        entities.begin(),
        entities.end(),
        [this](ECS::Entity left, ECS::Entity right)
        {
            const auto* leftIdentity =
                m_Registry.GetComponent<EntityIdentityComponent>(left);
            const auto* rightIdentity =
                m_Registry.GetComponent<EntityIdentityComponent>(right);
            return leftIdentity->id < rightIdentity->id;
        });

    return entities;
}

const SceneMetadata& Scene::GetMetadata() const noexcept
{
    return m_Metadata;
}

void Scene::SetName(std::string name)
{
    m_Metadata.name = std::move(name);
}

const ECS::Registry& Scene::GetRegistry() const noexcept
{
    return m_Registry;
}

void Scene::DetachFromParent(ECS::Entity entity)
{
    auto* hierarchy =
        m_Registry.GetComponent<HierarchyComponent>(entity);

    if (hierarchy == nullptr)
    {
        return;
    }

    const ECS::Entity parent = hierarchy->parent;

    if (!parent.IsValid())
    {
        hierarchy->previousSibling = ECS::Entity{};
        hierarchy->nextSibling = ECS::Entity{};
        return;
    }

    auto* parentHierarchy =
        m_Registry.GetComponent<HierarchyComponent>(parent);

    if (parentHierarchy != nullptr
        && parentHierarchy->firstChild == entity)
    {
        parentHierarchy->firstChild = hierarchy->nextSibling;
    }

    if (hierarchy->previousSibling.IsValid())
    {
        auto* previous =
            m_Registry.GetComponent<HierarchyComponent>(
                hierarchy->previousSibling);
        if (previous != nullptr)
        {
            previous->nextSibling = hierarchy->nextSibling;
        }
    }

    if (hierarchy->nextSibling.IsValid())
    {
        auto* next =
            m_Registry.GetComponent<HierarchyComponent>(
                hierarchy->nextSibling);
        if (next != nullptr)
        {
            next->previousSibling = hierarchy->previousSibling;
        }
    }

    hierarchy->parent = ECS::Entity{};
    hierarchy->previousSibling = ECS::Entity{};
    hierarchy->nextSibling = ECS::Entity{};
}

void Scene::DestroyEntityRecursive(ECS::Entity entity)
{
    auto* hierarchy =
        m_Registry.GetComponent<HierarchyComponent>(entity);

    if (hierarchy != nullptr)
    {
        std::vector<ECS::Entity> children;
        ECS::Entity child = hierarchy->firstChild;
        while (child.IsValid())
        {
            children.push_back(child);
            auto* childHierarchy =
                m_Registry.GetComponent<HierarchyComponent>(child);
            if (childHierarchy == nullptr)
            {
                break;
            }
            child = childHierarchy->nextSibling;
        }

        for (ECS::Entity childEntity : children)
        {
            DestroyEntityRecursive(childEntity);
        }

        DetachFromParent(entity);
    }

    const auto* identity =
        m_Registry.GetComponent<EntityIdentityComponent>(entity);
    if (identity != nullptr)
    {
        m_EntityIndex.erase(identity->id);
    }

    m_Registry.DestroyEntity(entity);
}

bool Scene::IsAncestor(
    ECS::Entity entity,
    ECS::Entity possibleAncestor) const
{
    ECS::Entity current = entity;

    while (current.IsValid())
    {
        const auto* hierarchy =
            m_Registry.GetComponent<HierarchyComponent>(current);

        if (hierarchy == nullptr)
        {
            return false;
        }

        if (hierarchy->parent == possibleAncestor)
        {
            return true;
        }

        current = hierarchy->parent;
    }

    return false;
}

} // namespace Janus
