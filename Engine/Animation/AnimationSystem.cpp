#include "Animation/AnimationSystem.h"
#include "Asset/AssetService.h"
#include "Scene/Scene.h"
#include "UI/UIComponents.h"
#include <cmath>
#include <set>

namespace Janus
{
namespace
{
bool HasTarget(const Scene& scene, ECS::Entity entity)
{
    return scene.HasComponent<SpriteRendererComponent>(entity) !=
           scene.HasComponent<ImageComponent>(entity);
}
} // namespace
Result<AnimationSystem::Playback> AnimationSystem::CreatePlayback(UUID id, AssetHandle clip)
{
    using Output = Result<Playback>;
    const auto entity = m_Scene.FindEntity(id);
    const auto* animator = m_Scene.GetComponent<AnimatorComponent>(entity);
    if (!animator || !animator->enabled || !HasTarget(m_Scene, entity))
        return Output::Failure(
            ErrorCode::InvalidState,
            "Enabled Animator needs exactly one SpriteRenderer or Image target.");
    auto valid = ValidateAnimator(*animator);
    if (!valid)
        return Output::Failure(valid.GetError());
    if (!clip.IsValid())
        clip = AssetHandle{animator->clip.id};
    auto loaded = m_Assets.LoadAnimationClip(clip);
    if (!loaded)
        return Output::Failure(loaded.GetError());
    Playback playback;
    playback.configuredClip = AssetHandle{animator->clip.id};
    playback.playOnStart = animator->playOnStart;
    // Playback survives cache eviction and neutral Step without accessing changed disk files.
    playback.data = *loaded.Value();
    playback.pose.clip = clip;
    playback.pose.texture = playback.data.texture;
    playback.pose.playing = true;
    playback.visible = true;
    Sample(playback);
    return Output::Success(std::move(playback));
}
void AnimationSystem::Sample(Playback& playback)
{
    f64 end = 0;
    usize index = 0;
    for (; index + 1 < playback.data.frames.size(); ++index)
    {
        end += playback.data.frames[index].duration;
        if (playback.pose.elapsedSeconds < end)
            break;
    }
    playback.pose.frameIndex = index;
    playback.pose.uvMin = playback.data.frames[index].uvMin;
    playback.pose.uvMax = playback.data.frames[index].uvMax;
}
Result<void> AnimationSystem::Reconcile()
{
    std::erase_if(m_Playback,
                  [this](const auto& entry)
                  {
                      const auto entity = m_Scene.FindEntity(entry.first);
                      const auto* animator = m_Scene.GetComponent<AnimatorComponent>(entity);
                      return !animator || !animator->enabled || !HasTarget(m_Scene, entity);
                  });
    std::map<UUID, AnimatorComponent> desired;
    for (const auto entity : m_Scene.GetEntities())
        if (const auto* animator = m_Scene.GetComponent<AnimatorComponent>(entity);
            animator && animator->enabled)
            desired.emplace(m_Scene.GetComponent<EntityIdentityComponent>(entity)->id, *animator);
    for (const auto& [id, animator] : desired)
    {
        auto valid = ValidateAnimator(animator);
        if (!valid)
            return valid;
        auto it = m_Playback.find(id);
        if (it != m_Playback.end() && it->second.configuredClip.id == animator.clip.id &&
            it->second.playOnStart == animator.playOnStart)
            continue;
        auto created = CreatePlayback(id, {});
        if (!created)
            return Result<void>::Failure(created.GetError());
        auto playback = std::move(created).Value();
        playback.visible = playback.pose.playing = animator.playOnStart;
        m_Playback.insert_or_assign(id, std::move(playback));
    }
    return Result<void>::Success();
}
Result<void> AnimationSystem::Start()
{
    if (m_Running)
        return Result<void>::Failure(ErrorCode::InvalidState, "Animation is already running.");
    std::set<AssetHandle> clips;
    for (const auto entity : m_Scene.GetEntities())
        if (const auto* animator = m_Scene.GetComponent<AnimatorComponent>(entity);
            animator && animator->enabled)
            clips.insert(AssetHandle{animator->clip.id});
    for (const auto clip : clips)
        m_Assets.Unload(clip);
    m_Running = true;
    auto result = Reconcile();
    if (!result)
        Stop();
    return result;
}
Result<void> AnimationSystem::Advance(TimeStep step)
{
    if (!m_Running)
        return Result<void>::Failure(ErrorCode::InvalidState, "Animation is not running.");
    if (!std::isfinite(step.GetSeconds()))
        return Result<void>::Failure(ErrorCode::InvalidArgument,
                                     "Animation timestep must be finite.");
    auto reconciled = Reconcile();
    if (!reconciled)
        return reconciled;
    for (auto& [id, playback] : m_Playback)
    {
        if (playback.fresh)
        {
            playback.fresh = false;
            continue;
        }
        if (!playback.pose.playing)
            continue;
        const auto* animator = m_Scene.GetComponent<AnimatorComponent>(m_Scene.FindEntity(id));
        const f64 speed = animator->speed;
        const f64 total = playback.data.duration;
        auto& time = playback.pose.elapsedSeconds;
        if (speed > 0)
        {
            if (playback.data.loop)
                time = std::fmod(time + std::fmod(step.GetSeconds(), total / speed) * speed, total);
            else if (step.GetSeconds() >= (total - time) / speed)
            {
                time = total;
                playback.pose.playing = false;
            }
            else
                time += step.GetSeconds() * speed;
        }
        Sample(playback);
    }
    return Result<void>::Success();
}
Result<void> AnimationSystem::Play(UUID entity, AssetHandle clip)
{
    if (!m_Running)
        return Result<void>::Failure(ErrorCode::InvalidState, "Animation is not running.");
    auto next = CreatePlayback(entity, clip);
    if (!next)
        return Result<void>::Failure(next.GetError());
    next.Value().fresh = true;
    m_Playback.insert_or_assign(entity, std::move(next).Value());
    return Result<void>::Success();
}
Result<void> AnimationSystem::Stop(UUID id)
{
    if (!m_Running)
        return Result<void>::Failure(ErrorCode::InvalidState, "Animation is not running.");
    const auto entity = m_Scene.FindEntity(id);
    if (!m_Scene.HasComponent<AnimatorComponent>(entity))
        return Result<void>::Failure(ErrorCode::InvalidState, "Entity has no Animator.");
    if (auto it = m_Playback.find(id); it != m_Playback.end())
    {
        it->second.visible = it->second.pose.playing = false;
        it->second.fresh = false;
    }
    return Result<void>::Success();
}
void AnimationSystem::Stop() noexcept
{
    m_Playback.clear();
    m_Running = false;
}
const AnimationPose* AnimationSystem::GetPose(UUID id) const noexcept
{
    const auto it = m_Playback.find(id);
    const auto entity = m_Scene.FindEntity(id);
    const auto* animator = m_Scene.GetComponent<AnimatorComponent>(entity);
    if (it == m_Playback.end() || !it->second.visible || !animator || !animator->enabled ||
        !HasTarget(m_Scene, entity))
        return nullptr;
    return &it->second.pose;
}
} // namespace Janus
