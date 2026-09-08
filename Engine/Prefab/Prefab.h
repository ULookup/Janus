#pragma once

#include "Asset/AssetRegistry.h"
#include "Scene/Command/EntityCommands.h"

#include <memory>

namespace Janus
{

class Prefab final
{
  public:
    static constexpr usize MaxBytes = 1024 * 1024;
    static constexpr usize MaxEntities = MaxAuthoringSubtreeEntities;
    static constexpr usize MaxDepth = MaxAuthoringSubtreeDepth;

    [[nodiscard]] static Result<std::string> Capture(Scene& scene, UUID root,
                                                     const ReflectionRegistry& reflection);
    [[nodiscard]] static Result<EntitySubtreeSnapshot> Parse(std::string_view text,
                                                             const ReflectionRegistry& reflection);
    [[nodiscard]] static Result<std::string>
    LoadRegistered(const AssetRegistry& assets, const std::filesystem::path& projectRoot,
                   AssetHandle asset, const ReflectionRegistry& reflection);
};

class InstantiatePrefabCommand final : public ICommand
{
  public:
    [[nodiscard]] static Result<std::unique_ptr<InstantiatePrefabCommand>>
    Create(Scene& scene, const ReflectionRegistry& reflection, std::string_view text);
    [[nodiscard]] UUID GetRoot() const noexcept
    {
        return m_Snapshot.root;
    }
    Result<void> Execute() override;
    Result<void> Undo() override;
    Result<void> Redo() override;
    Result<usize> EstimateUndoBytes() const override;
    std::vector<CommandEffect> GetEffects() const override;
    std::string_view Describe() const noexcept override
    {
        return "Instantiate Prefab";
    }

  private:
    InstantiatePrefabCommand(Scene& scene, const ReflectionRegistry& reflection,
                             EntitySubtreeSnapshot snapshot);
    Result<void> Restore();
    Scene& m_Scene;
    SceneReflection m_Reflection;
    EntitySubtreeSnapshot m_Snapshot;
    bool m_Executed = false;
    bool m_Present = false;
};

} // namespace Janus
