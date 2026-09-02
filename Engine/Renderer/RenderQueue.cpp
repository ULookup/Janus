#include "Renderer/RenderQueue.h"

#include <algorithm>

namespace Janus
{

void RenderQueue::Clear()
{
    m_Sprites.clear();
}

void RenderQueue::Submit(const Sprite& sprite)
{
    m_Sprites.push_back(sprite);
}

std::vector<Batch> RenderQueue::BuildBatches() const
{
    std::vector<Sprite> sorted = m_Sprites;

    std::stable_sort(
        sorted.begin(),
        sorted.end(),
        [](const Sprite& left, const Sprite& right)
        {
            if (left.layer != right.layer)
            {
                return left.layer < right.layer;
            }

            if (left.texture.value != right.texture.value)
            {
                return left.texture.value < right.texture.value;
            }

            return static_cast<int>(left.blendMode)
                < static_cast<int>(right.blendMode);
        });

    std::vector<Batch> batches;

    for (const Sprite& sprite : sorted)
    {
        if (batches.empty() ||
            batches.back().layer != sprite.layer ||
            batches.back().texture.value != sprite.texture.value ||
            batches.back().blendMode != sprite.blendMode)
        {
            Batch batch;
            batch.layer = sprite.layer;
            batch.texture = sprite.texture;
            batch.blendMode = sprite.blendMode;
            batches.push_back(std::move(batch));
        }

        batches.back().sprites.push_back(sprite);
    }

    return batches;
}

} // namespace Janus
