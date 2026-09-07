#include "Audio/AudioDevice.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

namespace Janus
{
namespace
{
class SDLAudioDevice final : public AudioDevice
{
  public:
    explicit SDLAudioDevice(SDL_AudioStream* stream) : m_Stream(stream) {}
    ~SDLAudioDevice() override
    {
        // Destroying this convenience stream closes its logical device before subsystem release.
        SDL_DestroyAudioStream(m_Stream);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    Result<void> Submit(std::span<const f32> samples) override
    {
        if (samples.size() > MaxBlockFrames * Channels || samples.size() % Channels != 0)
            return Result<void>::Failure(ErrorCode::InvalidArgument, "Audio block exceeds bounds.");
        const int queued = SDL_GetAudioStreamQueued(m_Stream);
        if (queued < 0)
            return Failure();
        constexpr int maxQueued = static_cast<int>(MaxBlockFrames * Channels * sizeof(f32) * 2);
        const int bytes = static_cast<int>(samples.size_bytes());
        if (queued + bytes > maxQueued && !SDL_ClearAudioStream(m_Stream))
            return Failure();
        if (!SDL_PutAudioStreamData(m_Stream, samples.data(), bytes))
            return Failure();
        return Result<void>::Success();
    }
    void Clear() noexcept override
    {
        static_cast<void>(SDL_ClearAudioStream(m_Stream));
    }

  private:
    static Result<void> Failure()
    {
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     std::string("Audio output: ") + SDL_GetError());
    }
    SDL_AudioStream* m_Stream;
};
} // namespace
Result<std::unique_ptr<AudioDevice>> CreateDefaultAudioDevice()
{
    using Output = Result<std::unique_ptr<AudioDevice>>;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        return Output::Failure(ErrorCode::PlatformInitFailed,
                               std::string("Audio device: ") + SDL_GetError());
    const SDL_AudioSpec spec{SDL_AUDIO_F32, AudioDevice::Channels, AudioDevice::SampleRate};
    auto* stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream)
    {
        const std::string message = SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return Output::Failure(ErrorCode::PlatformInitFailed, "Audio device: " + message);
    }
    auto device = std::make_unique<SDLAudioDevice>(stream);
    if (!SDL_ResumeAudioStreamDevice(stream))
        return Output::Failure(ErrorCode::PlatformInitFailed,
                               std::string("Audio resume: ") + SDL_GetError());
    return Output::Success(std::move(device));
}
} // namespace Janus
