#pragma once

#include "Core/Command/ICommand.h"
#include "Core/UUID/UUID.h"
#include "Scene/Command/SceneCommands.h"
#include "Scene/SceneReflection.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Janus
{

class Scene;

struct EntityAuthoringSnapshot
{
    UUID id;
    std::string name;
    std::optional<UUID> parent;
    usize siblingOrder = 0;
    std::vector<ReflectedComponentSnapshot> components;
};

struct EntitySubtreeSnapshot
{
    UUID root;
    std::vector<EntityAuthoringSnapshot> entities;
};

// Internal authoring snapshots use the host registry and persistent identities.
[[nodiscard]] Result<EntitySubtreeSnapshot>
CaptureEntitySubtree(Scene& scene, const SceneReflection& reflection, UUID root);
[[nodiscard]] Result<void> RestoreEntitySubtree(Scene& scene, const SceneReflection& reflection,
                                                const EntitySubtreeSnapshot& snapshot);

class ReparentEntityCommand final : public ICommand
{
  public:
    ReparentEntityCommand(Scene& scene, UUID entity, UUID parent, usize siblingIndex = 0);
    Result<void> Execute() override;
    Result<void> Undo() override;
    Result<void> Redo() override;
    std::string_view Describe() const noexcept override;
    Result<usize> EstimateUndoBytes() const override;
    std::vector<CommandEffect> GetEffects() const override;

  private:
    Result<void> Apply(UUID parent, usize index);
    Scene& m_Scene;
    UUID m_Entity;
    UUID m_Parent;
    usize m_Index;
    UUID m_OldParent;
    usize m_OldIndex = 0;
    bool m_Captured = false;
};

class CreateEntityCommand final : public ICommand
{
public:
    CreateEntityCommand(
        Scene& scene,
        UUID entity,
        std::string name);

    [[nodiscard]] Result<void> Execute() override;
    [[nodiscard]] Result<void> Undo() override;
    [[nodiscard]] Result<void> Redo() override;
    [[nodiscard]] std::string_view Describe() const noexcept override;
    Result<usize> EstimateUndoBytes() const override;
    std::vector<CommandEffect> GetEffects() const override;

  private:
    [[nodiscard]] Result<void> Create();

    Scene& m_Scene;
    UUID m_Entity;
    std::string m_Name;
};

class RenameEntityCommand final : public ICommand
{
public:
    RenameEntityCommand(
        Scene& scene,
        UUID entity,
        std::string name);

    [[nodiscard]] Result<void> Execute() override;
    [[nodiscard]] Result<void> Undo() override;
    [[nodiscard]] Result<void> Redo() override;
    [[nodiscard]] std::string_view Describe() const noexcept override;
    Result<usize> EstimateUndoBytes() const override;
    std::vector<CommandEffect> GetEffects() const override;

  private:
    [[nodiscard]] Result<void> ApplyName(
        const std::string& name);

    Scene& m_Scene;
    UUID m_Entity;
    std::string m_NewName;
    std::optional<std::string> m_OldName;
};

class DeleteEntityCommand final : public ICommand
{
public:
    DeleteEntityCommand(
        Scene& scene,
        SceneReflection reflection,
        UUID entity);

    [[nodiscard]] Result<void> Execute() override;
    [[nodiscard]] Result<void> Undo() override;
    [[nodiscard]] Result<void> Redo() override;
    [[nodiscard]] std::string_view Describe() const noexcept override;
    Result<usize> EstimateUndoBytes() const override;
    std::vector<CommandEffect> GetEffects() const override;

  private:
    [[nodiscard]] Result<EntitySubtreeSnapshot>
    CaptureSnapshot() const;

    [[nodiscard]] Result<void> RestoreSnapshot();
    void CleanupRestoredEntities() noexcept;

    Scene& m_Scene;
    SceneReflection m_Reflection;
    UUID m_Entity;
    std::optional<EntitySubtreeSnapshot> m_Snapshot;
};

} // namespace Janus
