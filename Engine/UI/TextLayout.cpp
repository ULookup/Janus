#include "UI/TextLayout.h"
#include <algorithm>
#include <cmath>

namespace Janus
{
namespace
{
Result<std::vector<u32>> Decode(std::string_view text)
{
    using Output = Result<std::vector<u32>>;
    auto invalid = []
    {
        return Output::Failure(
            ErrorCode::InvalidArgument,
            "Text requires valid UTF-8, at most 4096 bytes, and no controls except CR/LF.");
    };
    if (text.size() > 4096)
        return invalid();
    std::vector<u32> result;
    for (usize i = 0; i < text.size();)
    {
        const auto first = static_cast<u8>(text[i++]);
        u32 code = first;
        u32 count = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf)
        {
            code &= 0x1f;
            count = 1;
            minimum = 0x80;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            code &= 0xf;
            count = 2;
            minimum = 0x800;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            code &= 7;
            count = 3;
            minimum = 0x10000;
        }
        else if (first >= 0x80)
            return invalid();
        if (count > text.size() - i)
            return invalid();
        for (u32 n = 0; n < count; ++n)
        {
            const auto next = static_cast<u8>(text[i++]);
            if ((next & 0xc0) != 0x80)
                return invalid();
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
            (code < 32 && code != 10 && code != 13) || (code >= 0x7f && code <= 0x9f))
            return invalid();
        // CRLF is one line break; a lone CR also starts a new line.
        if (code == 13)
        {
            if (i < text.size() && text[i] == '\n')
                ++i;
            code = 10;
        }
        result.push_back(code);
    }
    return Output::Success(std::move(result));
}
} // namespace

Result<void> ValidateTextContent(std::string_view content)
{
    auto decoded = Decode(content);
    if (!decoded)
        return Result<void>::Failure(decoded.GetError());
    return Result<void>::Success();
}
Result<void> ValidateText(const TextComponent& text)
{
    if (!std::isfinite(text.fontSize) || text.fontSize <= 0 || text.fontSize > 1024 ||
        (text.alignment != "left" && text.alignment != "center" && text.alignment != "right"))
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Text fontSize must be in (0, 1024]; alignment must be left, center or right.");
    for (f32 channel : {text.color.r, text.color.g, text.color.b, text.color.a})
        if (!std::isfinite(channel) || channel < 0 || channel > 1)
            return Result<void>::Failure(ErrorCode::InvalidArgument, "Invalid Text color.");
    return ValidateTextContent(text.content);
}

Result<std::vector<Sprite>> TextLayout::Build(const TextComponent& text, const FontAsset& font,
                                              const UILayoutItem& item)
{
    using Output = Result<std::vector<Sprite>>;
    auto valid = ValidateText(text);
    if (!valid)
        return Output::Failure(valid.GetError());
    auto codes = Decode(text.content);
    if (!codes)
        return Output::Failure(codes.GetError());
    if (font.lineHeight <= 0 || !std::isfinite(font.lineHeight) || font.width == 0 ||
        font.height == 0 || !font.glyphs.contains(font.fallback))
        return Output::Failure(ErrorCode::InvalidArgument, "Text requires validated Font metrics.");
    const f32 scale = text.fontSize / font.lineHeight;
    auto glyphFor = [&](u32 code) -> const FontGlyph&
    {
        auto found = font.glyphs.find(code);
        return found == font.glyphs.end() ? font.glyphs.at(font.fallback) : found->second;
    };
    std::vector<Sprite> sprites;
    f32 top = item.rect.min.y;
    for (usize start = 0; start < codes.Value().size();)
    {
        usize end = start;
        f32 width = 0;
        while (end < codes.Value().size() && codes.Value()[end] != 10)
            width += glyphFor(codes.Value()[end++]).advance * scale;
        const f32 remaining = item.rect.max.x - item.rect.min.x - width;
        f32 pen = item.rect.min.x + (text.alignment == "right"    ? remaining
                                     : text.alignment == "center" ? remaining * 0.5f
                                                                  : 0);
        for (usize i = start; i < end; ++i)
        {
            const auto& glyph = glyphFor(codes.Value()[i]);
            const Vector2 min{pen + glyph.offsetX * scale,
                              top + (font.baseline + glyph.offsetY) * scale};
            const Vector2 max{min.x + glyph.width * scale, min.y + glyph.height * scale};
            const Vector2 clippedMin{std::max(min.x, item.visible.min.x),
                                     std::max(min.y, item.visible.min.y)};
            const Vector2 clippedMax{std::min(max.x, item.visible.max.x),
                                     std::min(max.y, item.visible.max.y)};
            if (clippedMin.x < clippedMax.x && clippedMin.y < clippedMax.y)
            {
                Sprite sprite;
                sprite.position = {(clippedMin.x + clippedMax.x) * 0.5f,
                                   (clippedMin.y + clippedMax.y) * 0.5f};
                sprite.size = {clippedMax.x - clippedMin.x, clippedMax.y - clippedMin.y};
                sprite.uv.min = {
                    (glyph.x + (clippedMin.x - min.x) / scale) / static_cast<f32>(font.width),
                    (glyph.y + (clippedMin.y - min.y) / scale) / static_cast<f32>(font.height)};
                sprite.uv.max = {
                    (glyph.x + (clippedMax.x - min.x) / scale) / static_cast<f32>(font.width),
                    (glyph.y + (clippedMax.y - min.y) / scale) / static_cast<f32>(font.height)};
                sprite.color = {text.color.r, text.color.g, text.color.b, text.color.a};
                sprites.push_back(sprite);
            }
            pen += glyph.advance * scale;
        }
        start = end + 1;
        top += text.fontSize;
    }
    return Output::Success(std::move(sprites));
}
} // namespace Janus
