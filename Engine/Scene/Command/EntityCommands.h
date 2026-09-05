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
