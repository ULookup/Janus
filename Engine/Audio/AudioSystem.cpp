#include "Audio/AudioSystem.h"
#include "Asset/AssetService.h"
#include "Core/Log/Log.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include <algorithm>
#include <cmath>

namespace Janus
{
const char* AudioPlaybackStatusName(AudioPlaybackStatus status) noexcept
{
    switch (status)
    {
    case AudioPlaybackStatus::Playing:
        return "Playing";
    case AudioPlaybackStatus::Paused:
        return "Paused";
    case AudioPlaybackStatus::Stopped:
        return "Stopped";
    }
    return "Stopped";
}
AudioSystem::AudioSystem(Scene& scene, AssetService& assets, AudioDeviceFactory factory)
    : m_Scene(scene), m_Assets(assets),
      m_Factory(factory ? std::move(factory) : AudioDeviceFactory{CreateDefaultAudioDevice})
{
}
AudioSystem::~AudioSystem()
{
    Stop();
}

Result<AudioSystem::Voice> AudioSystem::CreateVoice(UUID id, AssetHandle clip)
{
    using Output = Result<Voice>;
    const auto entity = m_Scene.FindEntity(id);
    const auto* source = m_Scene.GetComponent<AudioSourceComponent>(entity);
    if (!source || !source->enabled)
        return Output::Failure(ErrorCode::InvalidEntity, "Audio requires an enabled AudioSource.");
    auto valid = ValidateAudioSource(*source);
    if (!valid)
        return Output::Failure(valid.GetError());
    if (!clip.IsValid())
        clip = AssetHandle{source->clip.id};
    auto loaded = m_Assets.LoadAudioClip(clip);
    if (!loaded)
        return Output::Failure(loaded.GetError());
    Voice voice;
    voice.configuredClip = AssetHandle{source->clip.id};
    voice.data = std::move(loaded).Value();
    voice.state = {clip, AudioPlaybackStatus::Playing, 0, source->volume, source->loop};
    return Output::Success(std::move(voice));
}
Result<void> AudioSystem::Start()
{
    if (m_Running)
        return Result<void>::Failure(ErrorCode::InvalidState, "Audio already started.");
    m_OutputError.reset();
    m_FractionalFrames = 0;
    m_Scene.View<AudioSourceComponent>().ForEach(
        [&](ECS::Entity, const AudioSourceComponent& source)
        {
            if (source.enabled)
                m_Assets.Unload(AssetHandle{source.clip.id});
        });
    m_Running = true;
    auto result = Reconcile();
    if (!result)
        Stop();
    return result;
}
Result<void> AudioSystem::Reconcile()
{
    const auto removed = std::erase_if(m_Voices,
                                       [&](const auto& entry)
                                       {
                                           const auto* source =
                                               m_Scene.GetComponent<AudioSourceComponent>(
                                                   m_Scene.FindEntity(entry.first));
                                           return !source || !source->enabled;
                                       });
    if (removed)
        ClearOutput();
    usize count = 0;
    m_Scene.View<AudioSourceComponent>().ForEach(
        [&](ECS::Entity, const AudioSourceComponent& source)
        {
            if (source.enabled)
                ++count;
        });
    if (count > 64)
        return Result<void>::Failure(ErrorCode::InvalidArgument,
                                     "Audio supports at most 64 enabled sources.");
    auto result = Result<void>::Success();
    m_Scene.View<AudioSourceComponent, EntityIdentityComponent>().ForEach(
        [&](ECS::Entity, const AudioSourceComponent& source,
            const EntityIdentityComponent& identity)
        {
            if (!result || !source.enabled)
                return;
            auto valid = ValidateAudioSource(source);
            if (!valid)
            {
                result = valid;
                return;
            }
            const auto found = m_Voices.find(identity.id);
            if (found != m_Voices.end() && found->second.configuredClip.id == source.clip.id)
                return;
            auto voice = CreateVoice(identity.id, {});
            if (!voice)
            {
                result = Result<void>::Failure(voice.GetError());
                return;
            }
            if (!source.playOnStart)
                voice.Value().state.status = AudioPlaybackStatus::Stopped;
            if (found != m_Voices.end())
                ClearOutput();
            m_Voices.insert_or_assign(identity.id, std::move(voice).Value());
        });
    return result;
}
Result<AudioSystem::Voice*> AudioSystem::RequireVoice(UUID id)
{
    const auto* source = m_Scene.GetComponent<AudioSourceComponent>(m_Scene.FindEntity(id));
    const auto found = m_Voices.find(id);
    if (!m_Running || !source || !source->enabled || found == m_Voices.end())
        return Result<Voice*>::Failure(ErrorCode::InvalidState, "Audio source is not active.");
    return Result<Voice*>::Success(&found->second);
}
Result<void> AudioSystem::Play(UUID id, AssetHandle clip)
{
    auto old = RequireVoice(id);
    if (!old)
        return Result<void>::Failure(old.GetError());
    auto voice = CreateVoice(id, clip);
    if (!voice)
        return Result<void>::Failure(voice.GetError());
    // Runtime gain/loop survive a replay or clip switch; authoring stays untouched.
    voice.Value().state.volume = old.Value()->state.volume;
    voice.Value().state.loop = old.Value()->state.loop;
    ClearOutput();
    m_Voices.insert_or_assign(id, std::move(voice).Value());
    return Result<void>::Success();
}
Result<void> AudioSystem::Pause(UUID id)
{
    auto voice = RequireVoice(id);
    if (!voice)
        return Result<void>::Failure(voice.GetError());
    if (voice.Value()->state.status == AudioPlaybackStatus::Playing)
        voice.Value()->state.status = AudioPlaybackStatus::Paused;
    ClearOutput();
    return Result<void>::Success();
}
Result<void> AudioSystem::Resume(UUID id)
{
    auto voice = RequireVoice(id);
    if (!voice)
        return Result<void>::Failure(voice.GetError());
    if (voice.Value()->state.status == AudioPlaybackStatus::Paused)
        voice.Value()->state.status = AudioPlaybackStatus::Playing;
    return Result<void>::Success();
}
Result<void> AudioSystem::Stop(UUID id)
{
    auto voice = RequireVoice(id);
    if (!voice)
        return Result<void>::Failure(voice.GetError());
    voice.Value()->state.status = AudioPlaybackStatus::Stopped;
    voice.Value()->state.cursorSeconds = 0;
    ClearOutput();
    return Result<void>::Success();
}
Result<void> AudioSystem::SetVolume(UUID id, f32 volume)
{
    if (!std::isfinite(volume) || volume < 0 || volume > 1)
        return Result<void>::Failure(ErrorCode::InvalidArgument, "Audio volume must be in [0,1].");
    auto voice = RequireVoice(id);
    if (!voice)
        return Result<void>::Failure(voice.GetError());
    voice.Value()->state.volume = volume;
    return Result<void>::Success();
}
Result<void> AudioSystem::SetLoop(UUID id, bool loop)
{
    auto voice = RequireVoice(id);
    if (!voice)
        return Result<void>::Failure(voice.GetError());
    voice.Value()->state.loop = loop;
    return Result<void>::Success();
}
Result<void> AudioSystem::Advance(TimeStep step)
{
    const auto dt = step.GetSeconds();
    if (!std::isfinite(dt) || dt < 0)
        return Result<void>::Failure(ErrorCode::InvalidArgument, "Invalid audio timestep.");
    if (!m_Running)
        return Result<void>::Failure(ErrorCode::InvalidState, "Audio is stopped.");
    auto reconciled = Reconcile();
    if (!reconciled)
        return reconciled;
    const f64 framesExact = std::min(dt, 0.1) * AudioDevice::SampleRate + m_FractionalFrames;
    const usize frames = std::min(static_cast<usize>(framesExact), AudioDevice::MaxBlockFrames);
    m_FractionalFrames = framesExact - std::floor(framesExact);
    const bool audible =
        !m_Suspended && !m_OutputError && frames > 0 &&
        std::any_of(m_Voices.begin(), m_Voices.end(),
                    [](const auto& entry)
                    {
                        return entry.second.state.status == AudioPlaybackStatus::Playing &&
                               entry.second.state.volume > 0;
                    });
    std::vector<f32> mixed(audible ? frames * AudioDevice::Channels : 0, 0);
    for (auto& [id, voice] : m_Voices)
    {
        static_cast<void>(id);
        auto& state = voice.state;
        if (state.status != AudioPlaybackStatus::Playing)
            continue;
        const auto& clip = *voice.data;
        const auto length = clip.Duration();
        const usize sourceFrames = clip.samples.size() / clip.channels;
        if (audible && state.volume > 0)
        {
            for (usize frame = 0; frame < frames; ++frame)
            {
                f64 time = state.cursorSeconds + static_cast<f64>(frame) / AudioDevice::SampleRate;
                if (state.loop)
                    time = std::fmod(time, length);
                else if (time >= length)
                    break;
                const f64 position = time * clip.sampleRate;
                const usize first = std::min(static_cast<usize>(position), sourceFrames - 1);
                const usize second =
                    first + 1 < sourceFrames ? first + 1 : (state.loop ? 0 : first);
                const f32 blend = static_cast<f32>(position - static_cast<f64>(first));
                for (usize channel = 0; channel < AudioDevice::Channels; ++channel)
                {
                    const usize sourceChannel = clip.channels == 1 ? 0 : channel;
                    const f32 a = clip.samples[first * clip.channels + sourceChannel];
                    const f32 b = clip.samples[second * clip.channels + sourceChannel];
                    mixed[frame * 2 + channel] += (a + (b - a) * blend) * state.volume;
                }
            }
        }
        if (state.loop)
            state.cursorSeconds = std::fmod(state.cursorSeconds + std::fmod(dt, length), length);
        else
        {
            state.cursorSeconds = std::min(state.cursorSeconds + dt, length);
            if (state.cursorSeconds >= length)
                state.status = AudioPlaybackStatus::Stopped;
        }
    }
    if (audible)
    {
        if (!m_Device)
        {
            auto device = m_Factory();
            if (!device)
                FailOutput(device.GetError());
            else if (!device.Value())
                FailOutput(Error{ErrorCode::InvalidState, "Audio factory returned no device."});
            else
                m_Device = std::move(device).Value();
        }
        if (m_Device)
        {
            for (auto& sample : mixed)
                sample = std::clamp(sample, -1.0f, 1.0f);
            auto submitted = m_Device->Submit(mixed);
            if (!submitted)
                FailOutput(submitted.GetError());
        }
    }
    return Result<void>::Success();
}
void AudioSystem::ClearOutput() noexcept
{
    if (m_Device)
        m_Device->Clear();
}
void AudioSystem::FailOutput(Error error)
{
    // Preserve a bounded valid UTF-8 diagnostic, then release the failed device once.
    if (error.message.size() > 2048)
    {
        usize end = 2048;
        while (end && (static_cast<unsigned char>(error.message[end]) & 0xC0) == 0x80)
            --end;
        error.message.resize(end);
    }
    m_OutputError = std::move(error);
    JANUS_CORE_WARN("Audio output disabled: {}", m_OutputError->message);
    ClearOutput();
    m_Device.reset();
}
void AudioSystem::SetSuspended(bool suspended) noexcept
{
    m_Suspended = suspended;
    if (suspended)
        ClearOutput();
}
void AudioSystem::Stop() noexcept
{
    ClearOutput();
    m_Device.reset();
    m_Voices.clear();
    m_Running = false;
    m_FractionalFrames = 0;
}
const AudioPlaybackState* AudioSystem::GetState(UUID id) const noexcept
{
    const auto* source = m_Scene.GetComponent<AudioSourceComponent>(m_Scene.FindEntity(id));
    if (!source || !source->enabled)
        return nullptr;
    const auto found = m_Voices.find(id);
    return found == m_Voices.end() ? nullptr : &found->second.state;
}
} // namespace Janus
