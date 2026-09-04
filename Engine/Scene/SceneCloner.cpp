#include "Scene/SceneCloner.h"

#include "Scene/SceneDeserializer.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <utility>

namespace Janus
{

Result<std::unique_ptr<Scene>> SceneCloner::Clone(
    const Scene& source)
{
    auto serialized = SceneSerializer::Serialize(source);
    if (!serialized)
    {
        return Result<std::unique_ptr<Scene>>::Failure(
            serialized.GetError());
    }

    auto clone = SceneDeserializer::Deserialize(serialized.Value());
    if (!clone)
    {
        return Result<std::unique_ptr<Scene>>::Failure(
            clone.GetError());
    }

    return Result<std::unique_ptr<Scene>>::Success(
        std::move(clone).Value());
}

} // namespace Janus
