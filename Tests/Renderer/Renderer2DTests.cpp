#include "FakeRenderDevice.h"

#include "Renderer/Renderer2D.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Renderer2D clears queue and resets statistics on begin", "[renderer][renderer2d]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::Sprite sprite;
    sprite.texture = Janus::TextureHandle{1};

    renderer->SetViewport(Janus::Viewport{1280, 720});
    renderer->BeginFrame(Janus::OrthographicCamera{});
    renderer->SubmitSprite(sprite);
    renderer->BeginFrame(Janus::OrthographicCamera{});

    REQUIRE(renderer->GetStatistics().spriteCount == 0);
}

TEST_CASE("Renderer2D end frame draws submitted sprites", "[renderer][renderer2d]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::Sprite sprite;
    sprite.texture = Janus::TextureHandle{1};

    renderer->SetViewport(Janus::Viewport{1280, 720});
    renderer->BeginFrame(Janus::OrthographicCamera{});
    renderer->SubmitSprite(sprite);

    const auto result = renderer->EndFrame();

    REQUIRE(result);
    REQUIRE(device.drawCommands.size() == 1);
    REQUIRE(renderer->GetStatistics().spriteCount == 1);
    REQUIRE(renderer->GetStatistics().drawCallCount == 1);
}

TEST_CASE("Renderer2D creates and destroys textures", "[renderer][renderer2d]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    Janus::TextureDesc desc;
    desc.width = 1;
    desc.height = 1;

    const auto texture = renderer->CreateTexture(desc);

    REQUIRE(texture);
    REQUIRE(texture.Value().value != 0);

    renderer->DestroyTexture(texture.Value());
}
