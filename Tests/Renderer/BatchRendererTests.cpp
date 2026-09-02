#include "FakeRenderDevice.h"

#include "Renderer/BatchRenderer.h"
#include "Renderer/RenderQueue.h"
#include "Renderer/Sprite.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BatchRenderer flushes one batch for two sprites", "[renderer][batch]")
{
    Janus::Test::FakeRenderDevice device;
    Janus::BatchRenderer renderer(device);
    Janus::RenderQueue queue;

    Janus::Sprite a;
    a.texture = Janus::TextureHandle{1};
    Janus::Sprite b;
    b.texture = Janus::TextureHandle{1};

    queue.Submit(a);
    queue.Submit(b);

    Janus::RendererStatistics statistics;

    const auto result = renderer.Flush(
        queue.BuildBatches(),
        statistics);

    REQUIRE(result);
    REQUIRE(device.drawCommands.size() == 1);
    REQUIRE(statistics.spriteCount == 2);
    REQUIRE(statistics.batchCount == 1);
    REQUIRE(statistics.drawCallCount == 1);
    REQUIRE(statistics.textureBindCount == 1);
    REQUIRE(statistics.vertexCount == 8);
    REQUIRE(statistics.indexCount == 12);
}
