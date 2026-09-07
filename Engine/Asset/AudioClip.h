#pragma once
#include "Core/Types.h"
#include <vector>

namespace Janus
{
// Immutable once shared by AssetCache and runtime voices; no device resources.
struct AudioClip
{
    u32 sampleRate = 0;
    u32 channels = 0;
    std::vector<f32> samples;
    [[nodiscard]] f64 Duration() const noexcept
    {
        return sampleRate && channels ? static_cast<f64>(samples.size() / channels) / sampleRate
                                      : 0;
    }
};
} // namespace Janus
