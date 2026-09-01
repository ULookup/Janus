#pragma once

#include "Core/Error/Result.h"
#include "Core/Time/FrameClock.h"
#include "Platform/Window/WindowConfig.h"

#include <functional>
#include <memory>

namespace Janus
{

class Window;
class GraphicsContext;

namespace Detail
{

struct ApplicationDependencies
{
    std::function<Result<void>()> initializePlatform;
    std::function<void()> shutdownPlatform;
    std::function<Result<std::unique_ptr<Window>>(const WindowConfig&)> createWindow;
    std::function<Result<std::unique_ptr<GraphicsContext>>(Window&)> createGraphicsContext;
    std::function<FrameClock::TimePoint()> now;
};

[[nodiscard]] ApplicationDependencies CreateDefaultApplicationDependencies();
struct ApplicationTestAccess;

} // namespace Detail

} // namespace Janus
