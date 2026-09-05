#include "InspectorModel.h"

#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <utility>
#include <vector>

namespace Janus::Editor
{

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
