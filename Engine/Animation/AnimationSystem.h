#pragma once
#include "Animation/AnimatorComponent.h"
#include "Asset/AnimationClip.h"
#include "Core/Time/TimeStep.h"
#include <map>

namespace Janus
{
class Scene;
class AssetService;
struct AnimationPose
{
    AssetHandle clip;
    AssetHandle texture;
    Vector2 uvMin, uvMax;
    usize frameIndex = 0;
    f64 elapsedSeconds = 0;
    bool playing = false;
};

// Owner-thread runtime state; authoring components and cached assets are never mutated by playback.
class AnimationSystem final
{
  public:
    AnimationSystem(Scene& scene, AssetService& assets) : m_Scene(scene), m_Assets(assets) {}
    [[nodiscard]] Result<void> Start();
    [[nodiscard]] Result<void> Advance(TimeStep step);
    [[nodiscard]] Result<void> Play(UUID entity, AssetHandle clip = {});
    [[nodiscard]] Result<void> Stop(UUID entity);
    void Stop() noexcept;
    [[nodiscard]] const AnimationPose* GetPose(UUID entity) const noexcept;

  private:
    struct Playback
    {
        AssetHandle configuredClip;
        bool playOnStart = true;
        AnimationClip data;
        AnimationPose pose;
        bool visible = false;
        bool fresh = false;
    };
    [[nodiscard]] Result<void> Reconcile();
    [[nodiscard]] Result<Playback> CreatePlayback(UUID entity, AssetHandle clip);
    void Sample(Playback& playback);
    Scene& m_Scene;
    AssetService& m_Assets;
    std::map<UUID, Playback> m_Playback;
    bool m_Running = false;
};
} // namespace Janus
