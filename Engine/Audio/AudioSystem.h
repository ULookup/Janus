#pragma once
#include "Asset/AssetHandle.h"
#include "Asset/AudioClip.h"
#include "Audio/AudioDevice.h"
#include "Audio/AudioSourceComponent.h"
#include "Core/Time/TimeStep.h"
#include <map>
#include <optional>

namespace Janus
{
class Scene;
class AssetService;
enum class AudioPlaybackStatus
{
    Stopped,
    Playing,
    Paused
};
[[nodiscard]] const char* AudioPlaybackStatusName(AudioPlaybackStatus status) noexcept;
struct AudioPlaybackState
{
    AssetHandle clip;
    AudioPlaybackStatus status = AudioPlaybackStatus::Stopped;
    f64 cursorSeconds = 0;
    f32 volume = 1;
    bool loop = false;
};
class AudioSystem final
{
  public:
    AudioSystem(Scene& scene, AssetService& assets, AudioDeviceFactory factory = {});
    ~AudioSystem();
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
    [[nodiscard]] Result<void> Start();
    [[nodiscard]] Result<void> Advance(TimeStep step);
    [[nodiscard]] Result<void> Play(UUID entity, AssetHandle clip = {});
    [[nodiscard]] Result<void> Pause(UUID entity);
    [[nodiscard]] Result<void> Resume(UUID entity);
    [[nodiscard]] Result<void> Stop(UUID entity);
    [[nodiscard]] Result<void> SetVolume(UUID entity, f32 volume);
    [[nodiscard]] Result<void> SetLoop(UUID entity, bool loop);
    void SetSuspended(bool suspended) noexcept;
    void Stop() noexcept;
    [[nodiscard]] const AudioPlaybackState* GetState(UUID entity) const noexcept;
    [[nodiscard]] const std::optional<Error>& GetOutputError() const noexcept
    {
        return m_OutputError;
    }
    [[nodiscard]] bool IsOutputAvailable() const noexcept
    {
        return m_Device != nullptr;
    }

  private:
    struct Voice
    {
        AssetHandle configuredClip;
        std::shared_ptr<const AudioClip> data;
        AudioPlaybackState state;
    };
    [[nodiscard]] Result<Voice> CreateVoice(UUID entity, AssetHandle clip);
    [[nodiscard]] Result<Voice*> RequireVoice(UUID entity);
    [[nodiscard]] Result<void> Reconcile();
    void ClearOutput() noexcept;
    void FailOutput(Error error);
    Scene& m_Scene;
    AssetService& m_Assets;
    AudioDeviceFactory m_Factory;
    std::map<UUID, Voice> m_Voices;
    std::unique_ptr<AudioDevice> m_Device;
    std::optional<Error> m_OutputError;
    f64 m_FractionalFrames = 0;
    bool m_Running = false;
    bool m_Suspended = false;
};
} // namespace Janus
