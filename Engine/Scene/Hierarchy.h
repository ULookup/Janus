#pragma once

#include "ECS/Entity.h"

namespace Janus
{

struct HierarchyComponent
{
    ECS::Entity parent = ECS::Entity{};
    ECS::Entity firstChild = ECS::Entity{};
    ECS::Entity nextSibling = ECS::Entity{};
    ECS::Entity previousSibling = ECS::Entity{};
};

} // namespace Janus
