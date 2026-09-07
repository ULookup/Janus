#pragma once
#include "Asset/FontAsset.h"
#include "Renderer/Sprite.h"
#include "UI/UIComponents.h"
#include "UI/UILayout.h"
#include <string_view>

namespace Janus
{
[[nodiscard]] Result<void> ValidateTextContent(std::string_view content);
[[nodiscard]] Result<void> ValidateText(const TextComponent& text);

class TextLayout final
{
  public:
    // Returns ordered, clipped glyph sprites. The caller resolves the atlas texture.
    [[nodiscard]] static Result<std::vector<Sprite>>
    Build(const TextComponent& text, const FontAsset& font, const UILayoutItem& item);
};
} // namespace Janus
