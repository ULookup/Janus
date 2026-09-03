#pragma once

#include "Core/UUID/UUID.h"

#include <string>

namespace Janus
{

struct SceneMetadata
{
    UUID id;
    std::string name{"Scene"};
};

} // namespace Janus
