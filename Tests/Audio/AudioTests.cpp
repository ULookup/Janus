#include "../Asset/AssetTestUtils.h"
#include "../Renderer/FakeRenderDevice.h"
#include "Asset/AssetRegistry.h"
#include "Asset/AssetService.h"
#include "Asset/Loader/AudioClipLoader.h"
#include "Audio/AudioSystem.h"
#include "Core/Command/CommandBus.h"
#include "Core/FileSystem/FileSystem.h"
#include "Renderer/Renderer2D.h"
#include "Runtime/RuntimeExecution.h"
#include "Runtime/RuntimeSession.h"
#include "Scene/Command/SceneCommands.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneCloner.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneSerializer.h"
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <limits>

namespace
{
using namespace Janus;
std::vector<u8> Wave(u32 frames = 8000)
{
    std::vector<u8> bytes{'R', 'I', 'F', 'F'};
    auto word = [&](u32 value, int count)
    {
        for (int i = 0; i < count; ++i)
            bytes.push_back(static_cast<u8>(value >> (i * 8)));
    };
    word(36 + frames * 2, 4);
    for (char c : std::string("WAVEfmt "))
        bytes.push_back(static_cast<u8>(c));
    word(16, 4);
    word(1, 2);
    word(1, 2);
    word(8000, 4);
    word(16000, 4);
    word(2, 2);
    word(16, 2);
    for (char c : std::string("data"))
        bytes.push_back(static_cast<u8>(c));
    word(frames * 2, 4);
    for (u32 i = 0; i < frames; ++i)
        word(16384, 2);
    return bytes;
}
struct Capture
{
    std::vector<f32> samples;
    int clears = 0;
    int opens = 0;
    int closes = 0;
    bool fail = false;
};
class FakeAudio final : public AudioDevice
{
  public:
    explicit FakeAudio(Capture& capture) : m_Capture(capture)
    {
        ++m_Capture.opens;
    }
    ~FakeAudio() override
    {
        ++m_Capture.closes;
    }
    Result<void> Submit(std::span<const f32> samples) override
    {
        if (m_Capture.fail)
            return Result<void>::Failure(ErrorCode::InvalidState, "device lost");
        m_Capture.samples.insert(m_Capture.samples.end(), samples.begin(), samples.end());
        return Result<void>::Success();
    }
    void Clear() noexcept override
    {
        ++m_Capture.clears;
    }

  private:
    Capture& m_Capture;
};
AudioDeviceFactory Factory(Capture& capture)
{
    return [&capture]
    { return Result<std::unique_ptr<AudioDevice>>::Success(std::make_unique<FakeAudio>(capture)); };
}
struct Fixture
{
    Test::AssetTempDirectory temp;
    AssetRegistry registry;
    Test::FakeRenderDevice device;
    std::unique_ptr<Renderer2D> renderer = Detail::Renderer2DTestAccess::Create(device);
    AssetService assets{temp.Path(), registry, *renderer};
    Scene scene;
    ECS::Entity entity = scene.CreateEntity("Sound");
    UUID id = scene.GetComponent<EntityIdentityComponent>(entity)->id;
    AssetHandle clip;
    Capture capture;
    Fixture()
    {
        REQUIRE(FileSystem::WriteBinary(temp.Path() / "tone.wav", Wave()));
        auto registered = registry.Register(AssetType::AudioClip, "tone.wav");
        REQUIRE(registered);
        clip = registered.Value();
        REQUIRE(scene.AddComponent<AudioSourceComponent>(
            entity, {AssetReferenceValue{clip.id}, true, true, 0.5f, false}));
    }
};
} // namespace

TEST_CASE("Audio WAV validates chunks format alignment and bounded files", "[audio]")
{
    auto valid = AudioClipLoader::Parse(Wave());
    REQUIRE(valid);
    CHECK(valid.Value().sampleRate == 8000);
    CHECK(valid.Value().channels == 1);
    CHECK(valid.Value().Duration() == 1);
    CHECK(valid.Value().samples[0] == 0.5f);
    for (usize size : {0u, 11u, 20u, 43u, 100u})
    {
        auto bytes = Wave();
        bytes.resize(size);
        CHECK_FALSE(AudioClipLoader::Parse(bytes));
    }
    for (usize offset : {0u, 8u, 16u, 20u, 22u, 24u, 28u, 32u, 34u, 40u})
    {
        auto bytes = Wave();
        bytes[offset] = 255;
        CHECK_FALSE(AudioClipLoader::Parse(bytes));
    }
    CHECK_FALSE(AudioClipLoader::Parse(Wave(0)));
    CHECK_FALSE(AudioClipLoader::Parse(Wave(8000 * 121)));
    Test::AssetTempDirectory temp;
    CHECK_FALSE(AudioClipLoader::Load(temp.Path() / "absent.wav"));
    {
        std::ofstream large(temp.Path() / "large.wav", std::ios::binary);
        large.seekp(AudioClipLoader::MaxFileBytes);
        large.put('x');
    }
    CHECK_FALSE(AudioClipLoader::Load(temp.Path() / "large.wav"));
}

TEST_CASE("Audio stereo samples chunk extensions and duplicate chunks are validated", "[audio]")
{
    auto bytes = Wave();
    const auto word = [&](usize offset, u32 value, usize size)
    {
        for (usize i = 0; i < size; ++i)
            bytes[offset + i] = static_cast<u8>(value >> (8 * i));
    };
    word(22, 2, 2);
    word(28, 32000, 4);
    word(32, 4, 2);
    bytes[46] = 0;
    bytes[47] = 192; // Opposite polarity in the right channel.
    auto stereo = AudioClipLoader::Parse(bytes);
    REQUIRE(stereo);
    CHECK(stereo.Value().channels == 2);
    CHECK(stereo.Value().Duration() == 0.5);
    CHECK(stereo.Value().samples[0] == 0.5f);
    CHECK(stereo.Value().samples[1] == -0.5f);
    std::vector<u8> duplicate(bytes.begin() + 12, bytes.begin() + 36);
    bytes.insert(bytes.end(), duplicate.begin(), duplicate.end());
    word(4, static_cast<u32>(bytes.size() - 8), 4);
    CHECK_FALSE(AudioClipLoader::Parse(bytes));
    bytes = Wave();
    bytes.insert(bytes.end(), {'J', 'U', 'N', 'K', 1, 0, 0, 0, 42, 0});
    word(4, static_cast<u32>(bytes.size() - 8), 4);
    REQUIRE(AudioClipLoader::Parse(bytes));
    bytes.pop_back();
    word(4, static_cast<u32>(bytes.size() - 8), 4);
    CHECK_FALSE(AudioClipLoader::Parse(bytes));
    bytes = Wave();
    duplicate.assign(bytes.begin() + 36, bytes.end());
    bytes.insert(bytes.end(), duplicate.begin(), duplicate.end());
    word(4, static_cast<u32>(bytes.size() - 8), 4);
    CHECK_FALSE(AudioClipLoader::Parse(bytes));
}

TEST_CASE("Audio shares cached PCM saturates the mix and bounds source count", "[audio]")
{
    Fixture f;
    auto first = f.assets.LoadAudioClip(f.clip);
    auto second = f.assets.LoadAudioClip(f.clip);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first.Value() == second.Value());
    for (int i = 1; i < 64; ++i)
    {
        auto entity = f.scene.CreateEntity("Voice");
        REQUIRE(f.scene.AddComponent<AudioSourceComponent>(
            entity, {AssetReferenceValue{f.clip.id}, true, true, 1, true}));
    }
    AudioSystem audio(f.scene, f.assets, Factory(f.capture));
    REQUIRE(audio.Start());
    REQUIRE(audio.Advance(TimeStep::FromSeconds(0.01)));
    REQUIRE_FALSE(f.capture.samples.empty());
    CHECK(std::all_of(f.capture.samples.begin(), f.capture.samples.end(),
                      [](f32 s) { return s == 1; }));
    auto entity = f.scene.CreateEntity("Excess");
    REQUIRE(f.scene.AddComponent<AudioSourceComponent>(
        entity, {AssetReferenceValue{f.clip.id}, true, true, 1, true}));
    CHECK_FALSE(audio.Advance(TimeStep{}));
    audio.Stop();
    CHECK_FALSE(audio.Start());
    CHECK_FALSE(audio.GetState(f.id));
}

TEST_CASE("Audio mixes volume advances loops and preserves authoring", "[audio]")
{
    Fixture f;
    AudioSystem audio(f.scene, f.assets, Factory(f.capture));
    REQUIRE(audio.Start());
    CHECK(f.capture.opens == 0);
    REQUIRE(audio.Advance(TimeStep::FromSeconds(0.1)));
    REQUIRE(f.capture.samples.size() == 9600);
    CHECK(std::all_of(f.capture.samples.begin(), f.capture.samples.end(),
                      [](f32 sample) { return sample == 0.25f; }));
    CHECK(audio.GetState(f.id)->cursorSeconds == Catch::Approx(0.1));
    REQUIRE(audio.SetLoop(f.id, true));
    REQUIRE(audio.Advance(TimeStep::FromSeconds(1000000.25)));
    CHECK(audio.GetState(f.id)->cursorSeconds == Catch::Approx(0.35));
    CHECK(f.capture.samples.size() <= 19200);
    REQUIRE(audio.SetVolume(f.id, 0.2f));
    CHECK_FALSE(audio.SetVolume(f.id, std::numeric_limits<f32>::quiet_NaN()));
    CHECK(audio.GetState(f.id)->volume == 0.2f);
    CHECK(f.scene.GetComponent<AudioSourceComponent>(f.entity)->volume == 0.5f);
    REQUIRE(audio.SetLoop(f.id, false));
    REQUIRE(audio.Advance(TimeStep::FromSeconds(1)));
    CHECK(audio.GetState(f.id)->status == AudioPlaybackStatus::Stopped);
    CHECK(audio.GetState(f.id)->cursorSeconds == 1);
    REQUIRE(audio.Stop(f.id));
    CHECK(audio.GetState(f.id)->cursorSeconds == 0);
}

TEST_CASE("Audio pause silent step atomic play and cached lifetime", "[audio]")
{
    Fixture f;
    AudioSystem audio(f.scene, f.assets, Factory(f.capture));
    REQUIRE(audio.Start());
    REQUIRE(audio.Advance(TimeStep::FromSeconds(0.1)));
    REQUIRE(audio.Pause(f.id));
    REQUIRE(audio.Advance(TimeStep::FromSeconds(0.1)));
    CHECK(audio.GetState(f.id)->cursorSeconds == Catch::Approx(0.1));
    REQUIRE(audio.Resume(f.id));
    audio.SetSuspended(true);
    const auto samples = f.capture.samples.size();
    REQUIRE(audio.Advance(TimeStep::FromSeconds(1.0 / 60)));
    CHECK(f.capture.samples.size() == samples);
    CHECK(audio.GetState(f.id)->cursorSeconds == Catch::Approx(0.1 + 1.0 / 60));
    REQUIRE(f.assets.Unload(f.clip));
    REQUIRE(FileSystem::WriteText(f.temp.Path() / "tone.wav", "corrupt"));
    CHECK_FALSE(audio.Play(f.id));
    CHECK(audio.GetState(f.id)->cursorSeconds == Catch::Approx(0.1 + 1.0 / 60));
    REQUIRE(audio.Advance(TimeStep::FromSeconds(0.1)));
    audio.Stop();
    CHECK_FALSE(audio.GetState(f.id));
    CHECK(f.capture.closes == 1);
    CHECK_FALSE(audio.Start());
}

TEST_CASE("Audio unavailable and lost devices degrade to observable silent playback", "[audio]")
{
    Fixture f;
    AudioSystem audio(f.scene, f.assets,
                      []
                      {
                          return Result<std::unique_ptr<AudioDevice>>::Failure(
                              ErrorCode::PlatformInitFailed, "no device");
                      });
    REQUIRE(audio.Start());
    REQUIRE(audio.Advance(TimeStep::FromSeconds(0.1)));
    REQUIRE(audio.GetOutputError());
    CHECK(audio.GetOutputError()->message == "no device");
    CHECK(audio.GetState(f.id)->cursorSeconds == Catch::Approx(0.1));
    AudioSystem lost(f.scene, f.assets, Factory(f.capture));
    REQUIRE(lost.Start());
    f.capture.fail = true;
    REQUIRE(lost.Advance(TimeStep::FromSeconds(0.1)));
    CHECK(lost.GetOutputError());
    CHECK(f.capture.closes == 1);
    REQUIRE(lost.Advance(TimeStep::FromSeconds(0.1)));
    CHECK(f.capture.opens == 1);
}

TEST_CASE("Audio source removal disable and invalid timestep clean up predictably", "[audio]")
{
    Fixture f;
    AudioSystem audio(f.scene, f.assets, Factory(f.capture));
    REQUIRE(audio.Start());
    CHECK_FALSE(audio.Advance(TimeStep::FromSeconds(std::numeric_limits<f64>::infinity())));
    CHECK(audio.GetState(f.id)->cursorSeconds == 0);
    f.scene.GetComponent<AudioSourceComponent>(f.entity)->enabled = false;
    REQUIRE(audio.Advance(TimeStep{}));
    CHECK_FALSE(audio.GetState(f.id));
    CHECK_FALSE(audio.Play(f.id));
    f.scene.GetComponent<AudioSourceComponent>(f.entity)->enabled = true;
    REQUIRE(audio.Advance(TimeStep{}));
    CHECK(audio.GetState(f.id));
    REQUIRE(f.scene.DestroyEntity(f.entity));
    REQUIRE(audio.Advance(TimeStep{}));
    CHECK_FALSE(audio.GetState(f.id));
}

TEST_CASE("Audio authoring persists clones and undoes without runtime state", "[audio][reflection]")
{
    Fixture f;
    auto registry = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(registry);
    SceneReflection reflection(registry.Value());
    const auto type = MakeComponentTypeId("AudioSource");
    REQUIRE(registry.Value().FindComponent(type)->FindProperty("clip")->referenceConstraint ==
            "audio-clip");
    auto saved = SceneSerializer::Serialize(f.scene, registry.Value());
    REQUIRE(saved);
    CHECK(saved.Value().find("cursorSeconds") == std::string::npos);
    auto loaded = SceneDeserializer::Deserialize(saved.Value(), registry.Value());
    REQUIRE(loaded);
    auto clone = SceneCloner::Clone(*loaded.Value(), registry.Value());
    REQUIRE(clone);
    REQUIRE(clone.Value()->GetComponent<AudioSourceComponent>(clone.Value()->FindEntity(f.id)));
    CHECK(clone.Value()
              ->GetComponent<AudioSourceComponent>(clone.Value()->FindEntity(f.id))
              ->clip.id == f.clip.id);
    CommandBus commands;
    REQUIRE(commands.Execute(std::make_unique<SetPropertyCommand>(
        f.scene, reflection, f.id, type, MakePropertyId("AudioSource.volume"), f32{0.25f})));
    REQUIRE(commands.Undo());
    CHECK(f.scene.GetComponent<AudioSourceComponent>(f.entity)->volume == 0.5f);
    REQUIRE(commands.Redo());
    CHECK(f.scene.GetComponent<AudioSourceComponent>(f.entity)->volume == 0.25f);
    CHECK_FALSE(commands.Execute(std::make_unique<SetPropertyCommand>(
        f.scene, reflection, f.id, type, MakePropertyId("AudioSource.volume"), f32{-1})));
    REQUIRE(commands.Execute(
        std::make_unique<RemoveComponentCommand>(f.scene, reflection, f.id, type)));
    REQUIRE(commands.Undo());
    CHECK(f.scene.GetComponent<AudioSourceComponent>(f.entity)->clip.id == f.clip.id);
}

TEST_CASE("Audio Lua controls agree in shared execution and paused Runtime sessions",
          "[audio][runtime]")
{
    Fixture f;
    REQUIRE(FileSystem::WriteText(f.temp.Path() / "control.lua", R"lua(
return {
 OnCreate = function(self)
   self.entity:stop_audio()
   assert(not pcall(function() self.entity:play_audio('invalid') end))
   assert(not pcall(function() self.entity:set_audio_volume(-1) end))
   self.entity:set_audio_volume(0.3)
   self.entity:set_audio_loop(true)
   self.entity:play_audio()
   self.tick = 0
 end,
 OnUpdate = function(self)
   self.tick = self.tick + 1
   if self.tick == 2 then self.entity:pause_audio() end
   if self.tick == 4 then self.entity:resume_audio() end
   if self.tick == 8 then self.entity:stop_audio() end
   local status, cursor, volume, loop = self.entity:audio_state()
   local available, outputError = self.entity:audio_output()
   Diagnostics.publish_snapshot({audioStatus=status, audioCursor=cursor, audioVolume=volume,
      audioLoop=loop, audioAvailable=available, audioError=outputError})
 end
})lua"));
    auto script = f.registry.Register(AssetType::LuaScript, "control.lua");
    REQUIRE(script);
    REQUIRE(f.scene.AddComponent<LuaScriptComponent>(f.entity, {script.Value(), true}));
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    InputState input;
    Capture sessionCapture;
    auto session = RuntimeSession::Start(f.scene, reflection.Value(), f.assets, input, true, {},
                                         {1280, 720}, Factory(sessionCapture));
    REQUIRE(session);
    auto execution =
        RuntimeExecution::Create(f.scene, f.assets, input, {}, {1280, 720}, Factory(f.capture));
    REQUIRE(execution);
    REQUIRE(execution.Value()->Start());
    REQUIRE(f.assets.Unload(f.clip));
    REQUIRE(FileSystem::WriteText(f.temp.Path() / "tone.wav", "bad"));
    for (int frame = 0; frame < 8; ++frame)
    {
        REQUIRE(session.Value()->Step());
        REQUIRE(execution.Value()->Advance(TimeStep::FromSeconds(1.0 / 60), input,
                                           ScriptReloadPolicy::Skip));
        const auto* a = execution.Value()->GetAudio().GetState(f.id);
        const auto* b = session.Value()->GetAudio().GetState(f.id);
        REQUIRE(a);
        REQUIRE(b);
        CHECK(a->status == b->status);
        CHECK(a->cursorSeconds == b->cursorSeconds);
    }
    CHECK(sessionCapture.opens == 0);
    CHECK(f.capture.opens == 1);
    CHECK(session.Value()->GetSnapshot()->fields.contains("audioStatus"));
    REQUIRE(execution.Value()->Stop());
    REQUIRE(session.Value()->Stop());
    CHECK(f.capture.closes == 1);
    CHECK_FALSE(execution.Value()->Start());
    CHECK(f.scene.GetComponent<AudioSourceComponent>(f.entity)->volume == 0.5f);
}

TEST_CASE("Audio Runtime pause resume faults and failed teardown silence output",
          "[audio][runtime]")
{
    Fixture f;
    REQUIRE(FileSystem::WriteText(f.temp.Path() / "fault.lua", R"lua(
return {
 OnCreate=function(self) self.tick=0 end,
 OnUpdate=function(self) self.tick=self.tick+1 if self.tick==4 then error('audio fault') end end,
 OnDestroy=function(self) error('audio teardown') end
})lua"));
    auto script = f.registry.Register(AssetType::LuaScript, "fault.lua");
    REQUIRE(script);
    REQUIRE(f.scene.AddComponent<LuaScriptComponent>(f.entity, {script.Value(), true}));
    auto reflection = CreateBuiltinSceneReflectionRegistry();
    REQUIRE(reflection);
    InputState input;
    auto session = RuntimeSession::Start(f.scene, reflection.Value(), f.assets, input, false, {},
                                         {1280, 720}, Factory(f.capture));
    REQUIRE(session);
    REQUIRE(session.Value()->Update(TimeStep::FromSeconds(0.1)));
    const auto samples = f.capture.samples.size();
    const auto clears = f.capture.clears;
    REQUIRE(session.Value()->Pause());
    CHECK(f.capture.clears > clears);
    REQUIRE(session.Value()->Update(TimeStep::FromSeconds(1)));
    CHECK(session.Value()->GetAudio().GetState(f.id)->cursorSeconds == Catch::Approx(0.1));
    REQUIRE(session.Value()->Step());
    CHECK(f.capture.samples.size() == samples);
    REQUIRE(session.Value()->Resume());
    REQUIRE(session.Value()->Update(TimeStep::FromSeconds(0.1)));
    CHECK(f.capture.samples.size() > samples);
    const auto beforeFault = f.capture.clears;
    CHECK_FALSE(session.Value()->Update(TimeStep::FromSeconds(0.1)));
    CHECK(session.Value()->GetState() == RuntimeState::Faulted);
    CHECK(f.capture.clears > beforeFault);
    CHECK_FALSE(session.Value()->Stop());
    CHECK(f.capture.closes == 1);
    CHECK_FALSE(session.Value()->GetAudio().GetState(f.id));
}
