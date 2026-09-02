#pragma once

#include "Core/Types.h"

namespace Janus
{

struct RendererStatistics
{
    u32 spriteCount = 0;
    u32 batchCount = 0;
    u32 drawCallCount = 0;
    u32 textureBindCount = 0;
    u32 vertexCount = 0;
    u32 indexCount = 0;

    void Reset();
};

} // namespace Janus
