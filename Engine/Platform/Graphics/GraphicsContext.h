#pragma once

#include "Core/Error/Result.h"
#include "Core/Types.h"

#include <memory>

namespace Janus
{

class Window;

class GraphicsContext
{
public:
    virtual ~GraphicsContext() = default;

    GraphicsContext(const GraphicsContext&) = delete;
    GraphicsContext& operator=(const GraphicsContext&) = delete;

    [[nodiscard]]
    static Result<std::unique_ptr<GraphicsContext>> Create(Window& window);

    [[nodiscard]] virtual Result<void> MakeCurrent() = 0;
    [[nodiscard]] virtual Result<void> SetSwapInterval(i32 interval) = 0;
    virtual void Present() noexcept = 0;

protected:
    GraphicsContext() = default;
};

} // namespace Janus
