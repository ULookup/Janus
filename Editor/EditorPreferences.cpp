#include "EditorPreferences.h"

#include "Core/FileSystem/FileSystem.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string_view>

namespace Janus::Editor
{
namespace
{
using Json = nlohmann::json;
constexpr usize MaxBytes = 64 * 1024;

Result<void> Invalid()
{
    return Result<void>::Failure(
        ErrorCode::InvalidArgument,
        "Editor preferences contain unsupported, incorrectly typed or out-of-bounds data.");
}

bool ValidText(std::string_view text, usize maximum)
{
    if (text.empty() || text.size() > maximum)
        return false;
    for (usize index = 0; index < text.size();)
    {
        const auto first = static_cast<unsigned char>(text[index++]);
        if (first < 0x20 || first == 0x7f)
            return false;
        if (first < 0x80)
            continue;
        const usize extra = first >= 0xc2 && first <= 0xdf   ? 1
                            : first >= 0xe0 && first <= 0xef ? 2
                            : first >= 0xf0 && first <= 0xf4 ? 3
                                                             : 0;
        if (extra == 0 || extra > text.size() - index)
            return false;
        const auto second = static_cast<unsigned char>(text[index]);
        if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second >= 0xa0) ||
            (first == 0xf0 && second < 0x90) || (first == 0xf4 && second >= 0x90))
            return false;
        for (usize continuation = 0; continuation < extra; ++continuation)
            if ((static_cast<unsigned char>(text[index++]) & 0xc0) != 0x80)
                return false;
    }
    return true;
}

bool ValidPath(const std::filesystem::path& path)
{
    try
    {
        return ValidText(FileSystem::PathToUtf8(path), 4096);
    }
    catch (const std::filesystem::filesystem_error&)
    {
        // A malformed native path can fail UTF-8 conversion before JSON validation.
        return false;
    }
}

bool Fields(const Json& value, std::initializer_list<std::string_view> allowed)
{
    if (!value.is_object())
        return false;
    for (const auto& item : value.items())
        if (std::find(allowed.begin(), allowed.end(), item.key()) == allowed.end())
            return false;
    return true;
}

bool Number(double value, f32& destination, f32 minimum, f32 maximum, bool& clamped)
{
    if (!std::isfinite(value))
        return false;
    const auto bounded =
        std::clamp(value, static_cast<double>(minimum), static_cast<double>(maximum));
    clamped = clamped || bounded != value;
    destination = static_cast<f32>(bounded);
    return true;
}

bool ReadNumber(const Json& object, const char* name, f32& destination, f32 minimum, f32 maximum,
                bool& clamped)
{
    const auto found = object.find(name);
    return found == object.end() || (found->is_number() && Number(found->get<double>(), destination,
                                                                  minimum, maximum, clamped));
}

bool ReadPath(const Json& object, const char* name, std::filesystem::path& destination)
{
    const auto found = object.find(name);
    if (found == object.end() || !found->is_string())
        return false;
    const auto& value = found->get_ref<const std::string&>();
    if (!ValidText(value, 4096))
        return false;
    destination =
        std::filesystem::path(std::u8string(value.begin(), value.end())).lexically_normal();
    return true;
}

// Bound nesting before parsing so a tiny malicious document cannot exhaust parser/destructor
// stacks.
bool BoundedNesting(std::string_view text)
{
    int depth = 0;
    bool quoted = false, escaped = false;
    for (const char value : text)
    {
        if (quoted)
        {
            if (escaped)
                escaped = false;
            else if (value == '\\')
                escaped = true;
            else if (value == '"')
                quoted = false;
        }
        else if (value == '"')
            quoted = true;
        else if (value == '{' || value == '[')
        {
            if (++depth > 16)
                return false;
        }
        else if (value == '}' || value == ']')
            --depth;
    }
    return true;
}

const char* ModeName(EditorLayoutMode mode)
{
    return mode == EditorLayoutMode::Focus   ? "focus"
           : mode == EditorLayoutMode::Debug ? "debug"
                                             : "standard";
}
} // namespace

Result<std::filesystem::path>
ResolveEditorPreferenceProjectKey(const std::filesystem::path& projectRoot,
                                  const std::filesystem::path& baseDirectory)
{
    if (!ValidPath(projectRoot) || (!baseDirectory.empty() && !ValidPath(baseDirectory)))
        return Result<std::filesystem::path>::Failure(
            ErrorCode::InvalidArgument, "Preference project key requires a valid path.");
    std::error_code error;
    auto absolute = projectRoot;
    if (!absolute.is_absolute())
    {
        const auto base = baseDirectory.empty() ? std::filesystem::current_path(error)
                                                : std::filesystem::absolute(baseDirectory, error);
        if (error)
            return Result<std::filesystem::path>::Failure(ErrorCode::FileReadFailed,
                                                          error.message());
        absolute = base / projectRoot;
    }
    auto key = std::filesystem::weakly_canonical(absolute, error);
    if (error)
        return Result<std::filesystem::path>::Failure(ErrorCode::FileReadFailed, error.message());
    if (key.has_relative_path() && key.filename().empty())
        key = key.parent_path();
    if (!key.is_absolute() || !ValidPath(key))
        return Result<std::filesystem::path>::Failure(
            ErrorCode::InvalidArgument, "Preference project key must resolve to an absolute path.");
    return Result<std::filesystem::path>::Success(std::move(key));
}

Result<void> EditorPreferences::Normalize()
{
    if ((language != EditorLanguage::English && language != EditorLanguage::Chinese) ||
        (workspace.mode != EditorLayoutMode::Standard &&
         workspace.mode != EditorLayoutMode::Focus && workspace.mode != EditorLayoutMode::Debug) ||
        panelExpanded.size() > 64 || recentProjects.size() > 10 || cameras.size() > 32)
        return Invalid();
    if (!Number(userScale, userScale, 0.75f, 2, wasClamped) ||
        !Number(workspace.leftWidth, workspace.leftWidth, 170, 420, wasClamped) ||
        !Number(workspace.rightWidth, workspace.rightWidth, 250, 520, wasClamped) ||
        !Number(workspace.utilityHeight, workspace.utilityHeight, 120, 600, wasClamped))
        return Invalid();
    if (workspaceReferenceSize.x != 0 || workspaceReferenceSize.y != 0)
    {
        if (workspaceReferenceSize.x <= 0 || workspaceReferenceSize.y <= 0 ||
            !Number(workspaceReferenceSize.x, workspaceReferenceSize.x, 1, 32768, wasClamped) ||
            !Number(workspaceReferenceSize.y, workspaceReferenceSize.y, 1, 32768, wasClamped))
            return Invalid();
    }
    for (const auto& [key, expanded] : panelExpanded)
    {
        (void)expanded;
        if (!ValidText(key, 128))
            return Invalid();
    }
    for (auto& path : recentProjects)
    {
        if (!ValidPath(path))
            return Invalid();
        path = path.lexically_normal();
    }
    for (auto& camera : cameras)
    {
        if (!ValidPath(camera.projectRoot) || !ValidPath(camera.scenePath) ||
            !Number(camera.position.x, camera.position.x, -1e7f, 1e7f, wasClamped) ||
            !Number(camera.position.y, camera.position.y, -1e7f, 1e7f, wasClamped) ||
            !Number(camera.zoom, camera.zoom, 0.001f, 20, wasClamped))
            return Invalid();
        camera.projectRoot = camera.projectRoot.lexically_normal();
        camera.scenePath = camera.scenePath.lexically_normal();
    }
    return Result<void>::Success();
}

Result<EditorPreferences> EditorPreferences::Load(const std::filesystem::path& path)
{
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error)
        return Result<EditorPreferences>::Failure(ErrorCode::FileReadFailed, error.message());
    if (!exists)
        return Result<EditorPreferences>::Success(EditorPreferences{});
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return Result<EditorPreferences>::Failure(ErrorCode::FileReadFailed,
                                                  "Cannot open editor preferences.");
    std::string text(MaxBytes + 1, '\0');
    stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    const auto count = static_cast<usize>(stream.gcount());
    if (stream.bad() || count > MaxBytes)
        return Result<EditorPreferences>::Failure(
            ErrorCode::FileReadFailed, "Editor preferences exceed 64 KiB or could not be read.");
    text.resize(count);
    if (!BoundedNesting(text))
        return Result<EditorPreferences>::Failure(Invalid().GetError());
    const Json document = Json::parse(text, nullptr, false);
    if (!Fields(document,
                {"version", "userScale", "language", "workspace", "workspaceReferenceSize",
                 "showGrid", "panelExpanded", "recentProjects", "cameras"}) ||
        !document.contains("version") || !document["version"].is_number_integer() ||
        document["version"] != 1)
        return Result<EditorPreferences>::Failure(Invalid().GetError());
    EditorPreferences result;
    auto parse = [&]() -> bool
    {
        if (!ReadNumber(document, "userScale", result.userScale, 0.75f, 2, result.wasClamped))
            return false;
        if (document.contains("language"))
        {
            const auto& language = document["language"];
            if (language == "zh-CN")
                result.language = EditorLanguage::Chinese;
            else if (language != "en-US")
                return false;
        }
        if (document.contains("workspace"))
        {
            const auto& workspace = document["workspace"];
            if (!Fields(workspace, {"mode", "leftWidth", "rightWidth", "utilityHeight"}))
                return false;
            if (workspace.contains("mode"))
            {
                if (workspace["mode"] == "focus")
                    result.workspace = GetDefaultWorkspacePreferences(EditorLayoutMode::Focus);
                else if (workspace["mode"] == "debug")
                    result.workspace = GetDefaultWorkspacePreferences(EditorLayoutMode::Debug);
                else if (workspace["mode"] != "standard")
                    return false;
            }
            if (!ReadNumber(workspace, "leftWidth", result.workspace.leftWidth, 170, 420,
                            result.wasClamped) ||
                !ReadNumber(workspace, "rightWidth", result.workspace.rightWidth, 250, 520,
                            result.wasClamped) ||
                !ReadNumber(workspace, "utilityHeight", result.workspace.utilityHeight, 120, 600,
                            result.wasClamped))
                return false;
        }
        if (document.contains("workspaceReferenceSize"))
        {
            const auto& size = document["workspaceReferenceSize"];
            if (!Fields(size, {"x", "y"}) || !size.contains("x") || !size.contains("y") ||
                !ReadNumber(size, "x", result.workspaceReferenceSize.x, 0, 32768,
                            result.wasClamped) ||
                !ReadNumber(size, "y", result.workspaceReferenceSize.y, 0, 32768,
                            result.wasClamped))
                return false;
        }
        if (document.contains("showGrid"))
        {
            if (!document["showGrid"].is_boolean())
                return false;
            result.showGrid = document["showGrid"].get<bool>();
        }
        if (document.contains("panelExpanded"))
        {
            const auto& panels = document["panelExpanded"];
            if (!panels.is_object() || panels.size() > 64)
                return false;
            for (const auto& item : panels.items())
            {
                if (!ValidText(item.key(), 128) || !item.value().is_boolean())
                    return false;
                result.panelExpanded[item.key()] = item.value().get<bool>();
            }
        }
        if (document.contains("recentProjects"))
        {
            const auto& recent = document["recentProjects"];
            if (!recent.is_array() || recent.size() > 10)
                return false;
            for (const auto& project : recent)
            {
                std::filesystem::path parsed;
                if (!ReadPath(Json{{"path", project}}, "path", parsed))
                    return false;
                result.recentProjects.push_back(std::move(parsed));
            }
        }
        if (document.contains("cameras"))
        {
            const auto& cameras = document["cameras"];
            if (!cameras.is_array() || cameras.size() > 32)
                return false;
            for (const auto& camera : cameras)
            {
                EditorCameraPreference parsed;
                if (!Fields(camera, {"projectRoot", "scenePath", "position", "zoom"}) ||
                    !ReadPath(camera, "projectRoot", parsed.projectRoot) ||
                    !ReadPath(camera, "scenePath", parsed.scenePath) ||
                    !camera.contains("position") || !camera.contains("zoom"))
                    return false;
                const auto& position = camera["position"];
                if (!Fields(position, {"x", "y"}) || !position.contains("x") ||
                    !position.contains("y") ||
                    !ReadNumber(position, "x", parsed.position.x, -1e7f, 1e7f, result.wasClamped) ||
                    !ReadNumber(position, "y", parsed.position.y, -1e7f, 1e7f, result.wasClamped) ||
                    !ReadNumber(camera, "zoom", parsed.zoom, 0.001f, 20, result.wasClamped))
                    return false;
                result.cameras.push_back(std::move(parsed));
            }
        }
        return true;
    };
    if (!parse())
        return Result<EditorPreferences>::Failure(Invalid().GetError());
    auto normalized = result.Normalize();
    if (!normalized)
        return Result<EditorPreferences>::Failure(normalized.GetError());
    return Result<EditorPreferences>::Success(std::move(result));
}

Result<std::string> EditorPreferences::Serialize() const
{
    EditorPreferences normalized = *this;
    auto valid = normalized.Normalize();
    if (!valid)
        return Result<std::string>::Failure(valid.GetError());
    Json recent = Json::array(), views = Json::array();
    for (const auto& path : normalized.recentProjects)
        recent.push_back(FileSystem::PathToUtf8(path));
    for (const auto& camera : normalized.cameras)
        views.push_back({{"projectRoot", FileSystem::PathToUtf8(camera.projectRoot)},
                         {"scenePath", FileSystem::PathToUtf8(camera.scenePath)},
                         {"position", {{"x", camera.position.x}, {"y", camera.position.y}}},
                         {"zoom", camera.zoom}});
    Json document{
        {"version", 1},
        {"userScale", normalized.userScale},
        {"language", normalized.language == EditorLanguage::Chinese ? "zh-CN" : "en-US"},
        {"workspace",
         {{"mode", ModeName(normalized.workspace.mode)},
          {"leftWidth", normalized.workspace.leftWidth},
          {"rightWidth", normalized.workspace.rightWidth},
          {"utilityHeight", normalized.workspace.utilityHeight}}},
        {"workspaceReferenceSize",
         {{"x", normalized.workspaceReferenceSize.x}, {"y", normalized.workspaceReferenceSize.y}}},
        {"showGrid", normalized.showGrid},
        {"panelExpanded", normalized.panelExpanded},
        {"recentProjects", std::move(recent)},
        {"cameras", std::move(views)}};
    auto text = document.dump(2) + "\n";
    if (text.size() > MaxBytes)
        return Result<std::string>::Failure(ErrorCode::InvalidArgument,
                                            "Serialized editor preferences exceed 64 KiB.");
    return Result<std::string>::Success(std::move(text));
}

Result<void> EditorPreferences::Save(const std::filesystem::path& path) const
{
    const auto text = Serialize();
    if (!text)
        return Result<void>::Failure(text.GetError());
    std::error_code error;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), error);
    if (error)
        return Result<void>::Failure(ErrorCode::FileWriteFailed, error.message());
    return FileSystem::WriteTextAtomic(path, text.Value());
}

Result<void> EditorPreferences::RememberProject(const std::filesystem::path& projectRoot)
{
    if (!ValidPath(projectRoot))
        return Invalid();
    auto normalized = projectRoot.lexically_normal();
    std::erase_if(recentProjects,
                  [&](const auto& entry) { return entry.lexically_normal() == normalized; });
    recentProjects.insert(recentProjects.begin(), std::move(normalized));
    if (recentProjects.size() > 10)
        recentProjects.resize(10);
    return Result<void>::Success();
}

Result<void> EditorPreferences::RememberCamera(EditorCameraPreference camera)
{
    EditorPreferences candidate;
    candidate.cameras.push_back(std::move(camera));
    auto valid = candidate.Normalize();
    if (!valid)
        return valid;
    camera = std::move(candidate.cameras.front());
    std::erase_if(cameras,
                  [&](const auto& entry)
                  {
                      return entry.projectRoot.lexically_normal() == camera.projectRoot &&
                             entry.scenePath.lexically_normal() == camera.scenePath;
                  });
    cameras.insert(cameras.begin(), std::move(camera));
    if (cameras.size() > 32)
        cameras.resize(32);
    wasClamped = wasClamped || candidate.wasClamped;
    return Result<void>::Success();
}

const EditorCameraPreference*
EditorPreferences::FindCamera(const std::filesystem::path& projectRoot,
                              const std::filesystem::path& scenePath) const
{
    const auto project = projectRoot.lexically_normal();
    const auto scene = scenePath.lexically_normal();
    const auto found =
        std::find_if(cameras.begin(), cameras.end(), [&](const auto& camera)
                     { return camera.projectRoot == project && camera.scenePath == scene; });
    return found == cameras.end() ? nullptr : &*found;
}
} // namespace Janus::Editor
