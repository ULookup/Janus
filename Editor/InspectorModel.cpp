#include "InspectorModel.h"
#include "EditorActions.h"
#include "ProjectSession.h"

#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace Janus::Editor
{

namespace
{
bool SameValue(const PropertyValue& left, const PropertyValue& right)
{
    if (left.index() != right.index())
        return false;
    return std::visit(
        [&](const auto& value)
        {
            using T = std::decay_t<decltype(value)>;
            const auto& other = std::get<T>(right);
            if constexpr (std::is_same_v<T, Vector2>)
                return value.x == other.x && value.y == other.y;
            else if constexpr (std::is_same_v<T, ColorValue>)
                return value.r == other.r && value.g == other.g && value.b == other.b &&
                       value.a == other.a;
            else if constexpr (std::is_same_v<T, AssetReferenceValue>)
                return value.id == other.id;
            else
                return value == other;
        },
        left);
}
} // namespace

Result<void> InspectorEditDraft::BeginName(ProjectSession& project, UUID entity,
                                           std::string original)
{
    return Begin(project, entity, {}, {}, std::move(original), true);
}

Result<void> InspectorEditDraft::BeginProperty(ProjectSession& project, UUID entity,
                                               ComponentTypeId component, PropertyId property,
                                               PropertyValue original)
{
    return Begin(project, entity, component, property, std::move(original), false);
}

Result<void> InspectorEditDraft::Begin(ProjectSession& project, UUID entity,
                                       ComponentTypeId component, PropertyId property,
                                       PropertyValue original, bool name)
{
    if (m_Active && m_Project == project.GetProjectIdentity() && m_Entity == entity &&
        m_Component == component && m_Property == property && m_Name == name)
        return Result<void>::Success();
    if (IsEdited())
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Finish or discard the existing Inspector field draft first.");
    if (project.IsAuthoringReadOnly() || !project.GetEditorScene().FindEntity(entity).IsValid())
        return Result<void>::Failure(ErrorCode::InvalidState, "Inspector field is not editable.");
    m_Active = true;
    m_Name = name;
    m_Project = project.GetProjectIdentity();
    m_Entity = entity;
    m_Revision = project.GetSceneRevision();
    m_Generation = project.GetAuthoringGeneration();
    m_Component = component;
    m_Property = property;
    m_Original = original;
    m_Value = std::move(original);
    m_Error.reset();
    return Result<void>::Success();
}

void InspectorEditDraft::SetValue(PropertyValue value)
{
    if (m_Active)
        m_Value = std::move(value);
}

bool InspectorEditDraft::IsEdited() const
{
    return m_Active && !SameValue(m_Original, m_Value);
}

bool InspectorEditDraft::MatchesName(UUID entity) const noexcept
{
    return m_Active && m_Name && m_Entity == entity;
}

bool InspectorEditDraft::MatchesProperty(UUID entity, ComponentTypeId component,
                                         PropertyId property) const noexcept
{
    return m_Active && !m_Name && m_Entity == entity && m_Component == component &&
           m_Property == property;
}

void InspectorEditDraft::Cancel() noexcept
{
    m_Active = false;
    m_Error.reset();
}

Result<void> InspectorEditDraft::Commit(ProjectSession& project, EditorActions& actions)
{
    if (!IsEdited())
    {
        Cancel();
        return Result<void>::Success();
    }
    Result<void> result = Result<void>::Failure(
        ErrorCode::InvalidState,
        "Inspector authoring changed; discard this stale draft and edit the current value.");
    if (m_Project == project.GetProjectIdentity() && m_Revision == project.GetSceneRevision() &&
        m_Generation == project.GetAuthoringGeneration())
    {
        if (m_Name)
        {
            const auto* name = std::get_if<std::string>(&m_Value);
            if (name)
                result = actions.RenameEntityIfCurrent(m_Entity, *name, m_Revision, m_Generation);
            else
                result =
                    Result<void>::Failure(ErrorCode::InvalidArgument, "Entity name must be text.");
        }
        else
            result = actions.SetPropertyIfCurrent(m_Entity, m_Component, m_Property, m_Value,
                                                  m_Revision, m_Generation);
    }
    if (result)
        Cancel();
    else
        m_Error = result.GetError();
    return result;
}

Result<std::vector<InspectorComponentModel>>
BuildInspectorModel(
    const Scene& scene,
    UUID entity,
    const ReflectionRegistry& registry)
{
    if (!entity.IsValid())
    {
        return Result<std::vector<InspectorComponentModel>>::Failure(
            ErrorCode::InvalidArgument,
            "Inspector requires a valid entity UUID.");
    }

    if (!scene.FindEntity(entity).IsValid())
    {
        return Result<std::vector<InspectorComponentModel>>::Failure(
            ErrorCode::EntityNotFound,
            "Inspector entity no longer exists.");
    }

    SceneReflection reflection(registry);
    std::vector<InspectorComponentModel> model;

    for (const ComponentDescriptor* component :
         registry.GetComponents())
    {
        if (component == nullptr)
        {
            continue;
        }

        auto present = reflection.HasComponent(
            scene,
            entity,
            component->id);
        if (!present)
        {
            return Result<std::vector<InspectorComponentModel>>::Failure(
                present.GetError());
        }

        InspectorComponentModel componentModel;
        componentModel.descriptor = component;
        componentModel.present = present.Value();

        if (!componentModel.present
            && !component->removable)
        {
            return Result<std::vector<InspectorComponentModel>>::Failure(
                ErrorCode::InvalidState,
                "Inspector entity is missing required reflected component '"
                    + component->name
                    + "'.");
        }

        if (componentModel.present)
        {
            for (const PropertyDescriptor& property :
                 component->properties)
            {
                if (!property.visible)
                {
                    continue;
                }

                auto value = reflection.GetProperty(
                    scene,
                    entity,
                    component->id,
                    property.id);
                if (!value)
                {
                    return Result<std::vector<InspectorComponentModel>>::Failure(
                        value.GetError());
                }

                componentModel.properties.push_back(
                    InspectorPropertyModel{
                        &property,
                        std::move(value).Value()});
            }
        }

        model.push_back(
            std::move(componentModel));
    }

    return Result<std::vector<InspectorComponentModel>>::Success(
        std::move(model));
}

} // namespace Janus::Editor
