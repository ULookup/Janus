#pragma once

#include "Core/Time/TimeStep.h"
#include "Platform/Window/WindowConfig.h"

namespace Janus
{

struct ApplicationConfig
{
    WindowConfig window;
    TimeStep maximumFrameTime = TimeStep::FromMilliseconds(250.0);
};

} // namespace Janus
