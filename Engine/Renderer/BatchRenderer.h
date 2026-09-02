#pragma once

#include "Renderer/RenderDevice.h"
#include "Renderer/RenderQueue.h"
#include "Renderer/RendererStatistics.h"

#include <vector>

namespace Janus
{

class BatchRenderer
{
public:
    explicit BatchRenderer(RenderDevice& device);

    [[nodiscard]] Result<void> Flush(
        const std::vector<Batch>& batches,
        RendererStatistics& statistics);

private:
    RenderDevice& m_Device;
};

} // namespace Janus
