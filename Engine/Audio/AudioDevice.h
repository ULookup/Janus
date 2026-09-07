#pragma once
#include "Core/Error/Result.h"
#include "Core/Types.h"
#include <functional>
#include <memory>
#include <span>

namespace Janus
{
// Owner-thread PCM sink. Implementations must copy samples before Submit returns.
class AudioDevice
{
  public:
    static constexpr u32 SampleRate = 48000;
    static constexpr u32 Channels = 2;
    static constexpr usize MaxBlockFrames = SampleRate / 10;
    virtual ~AudioDevice() = default;
    [[nodiscard]] virtual Result<void> Submit(std::span<const f32> samples) = 0;
    virtual void Clear() noexcept = 0;
};
using AudioDeviceFactory = std::function<Result<std::unique_ptr<AudioDevice>>()>;
[[nodiscard]] Result<std::unique_ptr<AudioDevice>> CreateDefaultAudioDevice();
} // namespace Janus
