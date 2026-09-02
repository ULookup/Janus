#include "Renderer/RenderQueue.h"
#include "Renderer/Sprite.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("RenderQueue groups same layer and texture into one batch", "[renderer][queue]")
{
    Janus::RenderQueue queue;
    Janus::Sprite a;
    a.texture = Janus::TextureHandle{1};
    Janus::Sprite b;
    b.texture = Janus::TextureHandle{1};

    queue.Submit(a);
    queue.Submit(b);

    REQUIRE(queue.BuildBatches().size() == 1);
}

TEST_CASE("RenderQueue separates different textures", "[renderer][queue]")
{
    Janus::RenderQueue queue;
    Janus::Sprite a;
    a.texture = Janus::TextureHandle{1};
    Janus::Sprite b;
    b.texture = Janus::TextureHandle{2};

    queue.Submit(a);
    queue.Submit(b);

    REQUIRE(queue.BuildBatches().size() == 2);
}
