#pragma once

#include "Renderer/Sprite.h"

#include <vector>

namespace Janus
{

struct Batch
{
    i32 layer = 0;
    TextureHandle texture;
    BlendMode blendMode = BlendMode::Alpha;
    std::vector<Sprite> sprites;
};

class RenderQueue
{
public:
    void Clear();
    void Submit(const Sprite& sprite);

    [[nodiscard]] std::vector<Batch> BuildBatches() const;

private:
    std::vector<Sprite> m_Sprites;
};

} // namespace Janus
