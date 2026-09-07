#include "Asset/Loader/FontLoader.h"
#include "Core/FileSystem/FileSystem.h"
#include <cmath>
#include <nlohmann/json.hpp>

namespace Janus
{
namespace
{
using Json = nlohmann::json;
bool Number(const Json& object, const char* key, f32& value, f32 min, f32 max)
{
    auto it = object.find(key);
    if (it == object.end() || !it->is_number())
        return false;
    const auto number = it->get<double>();
    if (!std::isfinite(number) || number < min || number > max)
        return false;
    value = static_cast<f32>(number);
    return true;
}
bool Integer(const Json& object, const char* key, u32& value, u32 max)
{
    auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer())
        return false;
    const auto number = it->get<double>();
    if (number < 0 || number > max)
        return false;
    value = static_cast<u32>(number);
    return true;
}
bool Scalar(u32 value)
{
    return value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff);
}
} // namespace

Result<FontAsset> FontLoader::Parse(std::string_view source)
{
    using Output = Result<FontAsset>;
    auto invalid = []
    {
        return Output::Failure(ErrorCode::AssetDecodeFailed,
                               "Invalid Font v1 metadata: bounded atlas, metrics, unique Unicode "
                               "glyphs and fallback required.");
    };
    if (source.size() > 1024 * 1024)
        return invalid();
    const auto json = Json::parse(source, nullptr, false);
    if (!json.is_object())
        return invalid();
    FontAsset font;
    u32 version = 0;
    if (!Integer(json, "version", version, 1) || version != 1 ||
        !Integer(json, "width", font.width, 8192) || font.width == 0 ||
        !Integer(json, "height", font.height, 8192) || font.height == 0 ||
        !Number(json, "lineHeight", font.lineHeight, 1, 8192) ||
        !Number(json, "baseline", font.baseline, 0, font.lineHeight) ||
        !Integer(json, "fallback", font.fallback, 0x10ffff) || !Scalar(font.fallback))
        return invalid();
    auto atlas = json.find("atlas");
    auto glyphs = json.find("glyphs");
    if (atlas == json.end() || !atlas->is_string() || glyphs == json.end() || !glyphs->is_array() ||
        glyphs->empty() || glyphs->size() > 4096)
        return invalid();
    auto handle = AssetHandle::Parse(atlas->get_ref<const std::string&>());
    if (!handle)
        return invalid();
    font.atlas = handle.Value();
    for (const auto& entry : *glyphs)
    {
        FontGlyph glyph;
        u32 code = 0;
        if (!entry.is_object() || !Integer(entry, "codepoint", code, 0x10ffff) || !Scalar(code) ||
            !Number(entry, "x", glyph.x, 0, static_cast<f32>(font.width)) ||
            !Number(entry, "y", glyph.y, 0, static_cast<f32>(font.height)) ||
            !Number(entry, "width", glyph.width, 0, static_cast<f32>(font.width)) ||
            !Number(entry, "height", glyph.height, 0, static_cast<f32>(font.height)) ||
            !Number(entry, "offsetX", glyph.offsetX, -8192, 8192) ||
            !Number(entry, "offsetY", glyph.offsetY, -8192, 8192) ||
            !Number(entry, "advance", glyph.advance, 0, 8192) ||
            glyph.x + glyph.width > static_cast<f32>(font.width) ||
            glyph.y + glyph.height > static_cast<f32>(font.height) ||
            !font.glyphs.emplace(code, glyph).second)
            return invalid();
    }
    auto fallback = font.glyphs.find(font.fallback);
    if (fallback == font.glyphs.end() || fallback->second.width == 0 ||
        fallback->second.height == 0 || fallback->second.advance == 0)
        return invalid();
    return Output::Success(std::move(font));
}

Result<FontAsset> FontLoader::Load(const std::filesystem::path& path)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (!error && size > 1024 * 1024)
        return Result<FontAsset>::Failure(ErrorCode::AssetDecodeFailed,
                                          "Font metadata exceeds 1 MiB.");
    auto source = FileSystem::ReadText(path);
    if (!source)
        return Result<FontAsset>::Failure(source.GetError());
    return Parse(source.Value());
}
} // namespace Janus
