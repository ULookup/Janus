#include "Renderer/RendererStatistics.h"

namespace Janus
{

void RendererStatistics::Reset()
{
    spriteCount = 0;
    batchCount = 0;
    drawCallCount = 0;
    textureBindCount = 0;
    vertexCount = 0;
    indexCount = 0;
}

} // namespace Janus
