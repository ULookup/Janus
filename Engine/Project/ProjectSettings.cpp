#include "Project/ProjectSettings.h"
#include "Core/FileSystem/FileSystem.h"
#include <algorithm>
#include <cwctype>
#include <nlohmann/json.hpp>
#include <set>

namespace Janus
{
namespace
{
std::filesystem::path FromUtf8(std::string_view text)
{
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
using Json = nlohmann::json;
bool ValidUtf8(std::string_view text)
{
    for (usize i = 0; i < text.size();)
    {
        const auto first = static_cast<unsigned char>(text[i++]);
        if (first < 0x80)
            continue;
        u32 value = 0, minimum = 0;
        usize extra = 0;
        if (first >= 0xc2 && first <= 0xdf)
        {
            value = first & 0x1f;
            minimum = 0x80;
            extra = 1;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            value = first & 0x0f;
            minimum = 0x800;
            extra = 2;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            value = first & 0x07;
            minimum = 0x10000;
            extra = 3;
        }
        else
            return false;
        if (text.size() - i < extra)
            return false;
        while (extra-- != 0)
        {
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (next & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return false;
    }
    return true;
}
Result<void> Invalid(std::string message)
{
    return Result<void>::Failure(ErrorCode::InvalidArgument, std::move(message));
}
bool SamePart(const std::filesystem::path& a, const std::filesystem::path& b)
{
#ifdef _WIN32
    auto left = a.native(), right = b.native();
    std::transform(left.begin(), left.end(), left.begin(), std::towlower);
    std::transform(right.begin(), right.end(), right.begin(), std::towlower);
    return left == right;
#else
    return a == b;
#endif
}
Result<ProjectSettings> Decode(std::string_view text)
{
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
    auto failure = []
    {
        return Result<ProjectSettings>::Failure(
            ErrorCode::InvalidArgument,
            "Invalid project.json: expected version 1 and typed, known settings.");
    };
    if (doc.is_discarded() || duplicate || !doc.is_object() || !doc.contains("version") ||
        !doc["version"].is_number_integer() || doc["version"] != 1)
        return failure();
    const std::set<std::string> allowed{"version",   "name",       "defaultScene", "assetRegistry",
                                        "assetRoot", "scriptRoot", "width",        "height",
                                        "vsync",     "targetFps",  "inputBindings"};
    for (auto it = doc.begin(); it != doc.end(); ++it)
        if (!allowed.contains(it.key()))
            return failure();
    ProjectSettings result;
    for (auto field : {"name", "defaultScene", "assetRegistry", "assetRoot", "scriptRoot"})
        if (doc.contains(field) &&
            (!doc[field].is_string() || doc[field].get_ref<const std::string&>().size() > 1024))
            return failure();
    if (doc.contains("name"))
        result.name = doc["name"].get<std::string>();
    if (doc.contains("defaultScene"))
        result.defaultScene = FromUtf8(doc["defaultScene"].get<std::string>());
    if (doc.contains("assetRegistry"))
        result.assetRegistry = FromUtf8(doc["assetRegistry"].get<std::string>());
    if (doc.contains("assetRoot"))
        result.assetRoot = FromUtf8(doc["assetRoot"].get<std::string>());
    if (doc.contains("scriptRoot"))
        result.scriptRoot = FromUtf8(doc["scriptRoot"].get<std::string>());
    for (auto field : {"width", "height", "targetFps"})
        if (doc.contains(field) &&
            (!doc[field].is_number_integer() || doc[field] < 0 || doc[field] > 16384))
            return failure();
    if (doc.contains("width"))
        result.width = doc["width"].get<u32>();
    if (doc.contains("height"))
        result.height = doc["height"].get<u32>();
    if (doc.contains("targetFps"))
        result.targetFps = doc["targetFps"].get<u32>();
    if (doc.contains("vsync"))
    {
        if (!doc["vsync"].is_boolean())
            return failure();
        result.vsync = doc["vsync"].get<bool>();
    }
    if (doc.contains("inputBindings"))
    {
        if (!doc["inputBindings"].is_object() || doc["inputBindings"].size() > 64)
            return failure();
        for (auto it = doc["inputBindings"].begin(); it != doc["inputBindings"].end(); ++it)
        {
            if (!it.value().is_array() || it.value().empty() || it.value().size() > 8)
                return failure();
            auto& binding = result.inputBindings[it.key()];
            for (const auto& key : it.value())
            {
                if (!key.is_string())
                    return failure();
                auto parsed = ParseKeyCode(key.get_ref<const std::string&>());
                if (!parsed)
                    return failure();
                binding.push_back(*parsed);
            }
        }
    }
    return Result<ProjectSettings>::Success(std::move(result));
}
std::string Utf8(const std::filesystem::path& path)
{
    const auto text = path.generic_u8string();
    return std::string(text.begin(), text.end());
}
} // namespace
Result<std::filesystem::path> ResolveProjectPath(const std::filesystem::path& root,
                                                 const std::filesystem::path& relative)
{
    auto failure = []
    {
        return Result<std::filesystem::path>::Failure(
            ErrorCode::InvalidArgument, "Project path must resolve inside the project root.");
    };
    const auto text = Utf8(relative);
    if (root.empty() || relative.empty() || relative.is_absolute() || relative.has_root_name() ||
        relative.has_root_directory() || text.size() > 1024 ||
        text.find(':') != std::string::npos || text.find('\0') != std::string::npos)
        return failure();
    const auto normalized = relative.lexically_normal();
    if (normalized == ".")
        return failure();
    for (const auto& part : normalized)
        if (part == "..")
            return failure();
    std::error_code error;
    auto absoluteRoot = std::filesystem::weakly_canonical(root, error);
    if (error)
        return failure();
    auto target = std::filesystem::weakly_canonical(absoluteRoot / normalized, error);
    if (error)
        return failure();
    auto cursor = target.begin();
    for (const auto& part : absoluteRoot)
    {
        if (cursor == target.end() || !SamePart(part, *cursor))
            return failure();
        ++cursor;
    }
    if (cursor == target.end())
        return failure();
    return Result<std::filesystem::path>::Success(std::move(target));
}
Result<void> ValidateProjectSettings(const std::filesystem::path& root,
                                     const ProjectSettings& settings)
{
    if (settings.name.empty() || settings.name.size() > 128 || !ValidUtf8(settings.name) ||
        std::any_of(settings.name.begin(), settings.name.end(),
                    [](unsigned char c) { return c < 32; }) ||
        settings.width == 0 || settings.height == 0 || settings.width > 16384 ||
        settings.height > 16384 || settings.targetFps > 1000)
        return Invalid("Invalid project name, resolution or target FPS (0-1000).");
    for (const auto* path : {&settings.defaultScene, &settings.assetRegistry, &settings.assetRoot,
                             &settings.scriptRoot})
    {
        auto resolved = ResolveProjectPath(root, *path);
        if (!resolved)
            return Result<void>::Failure(resolved.GetError());
    }
    return ValidateInputBindings(settings.inputBindings);
}
Result<ProjectSettings> LoadProjectSettings(const ProjectRuntimeConfig& config)
{
    auto path = ResolveProjectPath(config.root, "project.json");
    if (!path)
        return Result<ProjectSettings>::Failure(path.GetError());
    std::error_code error;
    const bool exists = std::filesystem::exists(path.Value(), error);
    if (error)
        return Result<ProjectSettings>::Failure(ErrorCode::InvalidArgument,
                                                "Cannot inspect project.json.");
    ProjectSettings settings;
    if (exists)
    {
        const auto size = std::filesystem::file_size(path.Value(), error);
        if (error || size > 65536)
            return Result<ProjectSettings>::Failure(
                ErrorCode::InvalidArgument, "project.json exceeds 64 KiB or is not a file.");
        auto text = FileSystem::ReadText(path.Value());
        if (!text)
            return Result<ProjectSettings>::Failure(text.GetError());
        if (text.Value().size() > 65536)
            return Result<ProjectSettings>::Failure(ErrorCode::InvalidArgument,
                                                    "project.json exceeds 64 KiB.");
        auto decoded = Decode(text.Value());
        if (!decoded)
            return decoded;
        settings = std::move(decoded).Value();
        auto valid = ValidateProjectSettings(config.root, settings);
        if (!valid)
            return Result<ProjectSettings>::Failure(valid.GetError());
    }
    if (config.assetRegistryPath)
        settings.assetRegistry = *config.assetRegistryPath;
    if (config.startupScenePath)
        settings.defaultScene = *config.startupScenePath;
    auto valid = ValidateProjectSettings(config.root, settings);
    if (!valid)
        return Result<ProjectSettings>::Failure(valid.GetError());
    return Result<ProjectSettings>::Success(std::move(settings));
}
Result<void> SaveProjectSettings(const std::filesystem::path& root, const ProjectSettings& settings)
{
    auto valid = ValidateProjectSettings(root, settings);
    if (!valid)
        return valid;
    auto path = ResolveProjectPath(root, "project.json");
    if (!path)
        return Result<void>::Failure(path.GetError());
    Json bindings = Json::object();
    for (const auto& [name, keys] : settings.inputBindings)
    {
        bindings[name] = Json::array();
        for (auto key : keys)
            bindings[name].push_back(KeyCodeName(key));
    }
    Json doc{{"version", 1},
             {"name", settings.name},
             {"defaultScene", Utf8(settings.defaultScene)},
             {"assetRegistry", Utf8(settings.assetRegistry)},
             {"assetRoot", Utf8(settings.assetRoot)},
             {"scriptRoot", Utf8(settings.scriptRoot)},
             {"width", settings.width},
             {"height", settings.height},
             {"vsync", settings.vsync},
             {"targetFps", settings.targetFps},
             {"inputBindings", bindings}};
    return FileSystem::WriteTextAtomic(path.Value(), doc.dump(2) + "\n");
}
} // namespace Janus
