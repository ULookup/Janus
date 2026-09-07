#include "Asset/Loader/AnimationClipLoader.h"
#include "Core/FileSystem/FileSystem.h"
#include <cmath>
#include <nlohmann/json.hpp>

namespace Janus
{
namespace
{
using Json = nlohmann::json;
bool UV(const Json& object, const char* key, Vector2& result)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_array() || it->size() != 2)
        return false;
    for (usize i = 0; i < 2; ++i)
    {
        if (!(*it)[i].is_number())
            return false;
        const auto value = (*it)[i].get<f64>();
        if (!std::isfinite(value) || value < 0 || value > 1)
            return false;
        (i == 0 ? result.x : result.y) = static_cast<f32>(value);
    }
    return true;
}
} // namespace

Result<AnimationClip> AnimationClipLoader::Parse(std::string_view source)
{
    using Output = Result<AnimationClip>;
    auto invalid = []
    {
        return Output::Failure(
            ErrorCode::AssetDecodeFailed,
            "AnimationClip v1 requires a texture, loop, 1-1024 bounded UV frames, durations "
            "0.001-60 seconds, and total duration <=3600 seconds.");
    };
    if (source.size() > 1024 * 1024)
        return invalid();
    const auto json = Json::parse(source, nullptr, false);
    if (!json.is_object() || !json.contains("version") || !json["version"].is_number_integer() ||
        json["version"] != 1 || !json.contains("texture") || !json["texture"].is_string() ||
        !json.contains("loop") || !json["loop"].is_boolean() || !json.contains("frames") ||
        !json["frames"].is_array() || json["frames"].empty() || json["frames"].size() > 1024)
        return invalid();
    auto texture = AssetHandle::Parse(json["texture"].get_ref<const std::string&>());
    if (!texture)
        return invalid();
    AnimationClip clip;
    clip.texture = texture.Value();
    clip.loop = json["loop"].get<bool>();
    for (const auto& entry : json["frames"])
    {
        AnimationFrame frame;
        if (!entry.is_object() || !UV(entry, "uvMin", frame.uvMin) ||
            !UV(entry, "uvMax", frame.uvMax) || frame.uvMin.x >= frame.uvMax.x ||
            frame.uvMin.y >= frame.uvMax.y || !entry.contains("duration") ||
            !entry["duration"].is_number())
            return invalid();
        frame.duration = entry["duration"].get<f64>();
        if (!std::isfinite(frame.duration) || frame.duration < 0.001 || frame.duration > 60)
            return invalid();
        clip.duration += frame.duration;
        if (clip.duration > 3600)
            return invalid();
        clip.frames.push_back(frame);
    }
    return Output::Success(std::move(clip));
}

Result<AnimationClip> AnimationClipLoader::Load(const std::filesystem::path& path)
{
    std::error_code error;
    if (std::filesystem::file_size(path, error) > 1024 * 1024 && !error)
        return Result<AnimationClip>::Failure(ErrorCode::AssetDecodeFailed,
                                              "AnimationClip exceeds 1 MiB.");
    auto source = FileSystem::ReadText(path);
    if (!source)
        return Result<AnimationClip>::Failure(source.GetError());
    return Parse(source.Value());
}
} // namespace Janus
