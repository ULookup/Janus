#include "FakeRenderDevice.h"

#include "Renderer/Renderer2D.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "Renderer2D clears queue and resets statistics on begin",
    "[renderer][renderer2d]")
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

TEST_CASE(
    "Renderer2D end frame draws submitted sprites",
    "[renderer][renderer2d]")
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

TEST_CASE(
    "Renderer2D creates and destroys textures",
    "[renderer][renderer2d]")
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

TEST_CASE(
    "Renderer2D creates an offscreen render target with blank color storage",
    "[renderer][renderer2d][render-target][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    const auto target =
        renderer->CreateRenderTarget(
            Janus::RenderTargetDesc{640, 360});

    REQUIRE(target);
    REQUIRE(target.Value().value != 0);
    REQUIRE(device.createdTextures.size() == 1);
    REQUIRE(device.createdTextures[0].width == 640);
    REQUIRE(device.createdTextures[0].height == 360);
    REQUIRE(device.createdTextures[0].dataSize == 0);
    REQUIRE(device.createdTextures[0].pixels.empty());

    REQUIRE(device.createdFramebuffers.size() == 1);
    REQUIRE(
        device.createdFramebuffers[0].desc.colorTexture.value
        == device.createdTextures[0].handle.value);
    REQUIRE(device.createdFramebuffers[0].desc.width == 640);
    REQUIRE(device.createdFramebuffers[0].desc.height == 360);

    const auto color =
        renderer->GetRenderTargetColorTexture(target.Value());
    REQUIRE(color);
    REQUIRE(
        color.Value().value
        == device.createdTextures[0].handle.value);

    const auto presentation =
        renderer->GetRenderTargetPresentationHandle(
            target.Value());
    REQUIRE(presentation);
    REQUIRE(
        presentation.Value().value
        == static_cast<Janus::usize>(color.Value().value));
}

TEST_CASE(
    "Renderer2D binds target frame and restores default framebuffer",
    "[renderer][renderer2d][render-target][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    const auto target =
        renderer->CreateRenderTarget(
            Janus::RenderTargetDesc{800, 450});
    REQUIRE(target);

    Janus::RenderFrameDesc frame;
    frame.viewport = Janus::Viewport{800, 450};
    frame.target = target.Value();
    frame.clearColor = Janus::Color{
        0.1f,
        0.2f,
        0.3f,
        1.0f};

    REQUIRE(renderer->BeginFrame(frame));
    REQUIRE(device.boundFramebuffers.size() == 1);
    REQUIRE(
        device.boundFramebuffers[0].value
        == device.createdFramebuffers[0].handle.value);
    REQUIRE(device.lastViewport.width == 800);
    REQUIRE(device.lastViewport.height == 450);

    REQUIRE(renderer->EndFrame());
    REQUIRE(device.defaultFramebufferBindCount == 1);
    REQUIRE(device.lastClearColor.r == frame.clearColor.r);
    REQUIRE(device.lastClearColor.g == frame.clearColor.g);
    REQUIRE(device.lastClearColor.b == frame.clearColor.b);
}

TEST_CASE(
    "Renderer2D resizes render targets transactionally",
    "[renderer][renderer2d][render-target][v0.6]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    const auto target =
        renderer->CreateRenderTarget(
            Janus::RenderTargetDesc{320, 180});
    REQUIRE(target);

    const auto originalColor =
        renderer->GetRenderTargetColorTexture(target.Value());
    REQUIRE(originalColor);

    const auto originalFramebuffer =
        device.createdFramebuffers[0].handle;

    REQUIRE(
        renderer->ResizeRenderTarget(
            target.Value(),
            1280,
            720));

    REQUIRE(device.createdTextures.size() == 2);
    REQUIRE(device.createdFramebuffers.size() == 2);
    REQUIRE(device.destroyedFramebuffers.size() == 1);
    REQUIRE(device.destroyedTextures.size() == 1);
    REQUIRE(
        device.destroyedFramebuffers[0].value
        == originalFramebuffer.value);
    REQUIRE(
        device.destroyedTextures[0].value
        == originalColor.Value().value);
    REQUIRE(device.destructionOrder.size() == 2);
    REQUIRE(device.destructionOrder[0] == 'F');
    REQUIRE(device.destructionOrder[1] == 'T');

    const auto resizedColor =
        renderer->GetRenderTargetColorTexture(target.Value());
    REQUIRE(resizedColor);
    REQUIRE(
        resizedColor.Value().value
        != originalColor.Value().value);

    Janus::RenderFrameDesc frame;
    frame.viewport = Janus::Viewport{1280, 720};
    frame.target = target.Value();

    REQUIRE(renderer->BeginFrame(frame));
    REQUIRE(renderer->EndFrame());
}

TEST_CASE(
    "Renderer2D preserves old target when replacement framebuffer fails",
    "[renderer][renderer2d][render-target][v0.6][errors]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    const auto target =
        renderer->CreateRenderTarget(
            Janus::RenderTargetDesc{400, 300});
    REQUIRE(target);

    const auto originalColor =
        renderer->GetRenderTargetColorTexture(target.Value());
    REQUIRE(originalColor);

    device.failNextFramebufferCreate = true;

    const auto resized =
        renderer->ResizeRenderTarget(
            target.Value(),
            1024,
            768);

    REQUIRE_FALSE(resized);
    REQUIRE(
        resized.GetError().code
        == Janus::ErrorCode::FramebufferCreateFailed);
    REQUIRE(device.createdFramebuffers.size() == 1);
    REQUIRE(device.destroyedFramebuffers.empty());

    const auto currentColor =
        renderer->GetRenderTargetColorTexture(target.Value());
    REQUIRE(currentColor);
    REQUIRE(
        currentColor.Value().value
        == originalColor.Value().value);

    REQUIRE(device.createdTextures.size() == 2);
    REQUIRE(device.destroyedTextures.size() == 1);
    REQUIRE(
        device.destroyedTextures[0].value
        == device.createdTextures[1].handle.value);
}

TEST_CASE(
    "Renderer2D rejects invalid or active render target operations",
    "[renderer][renderer2d][render-target][v0.6][errors]")
{
    Janus::Test::FakeRenderDevice device;
    auto renderer = Janus::Detail::Renderer2DTestAccess::Create(device);

    const auto invalidCreate =
        renderer->CreateRenderTarget(
            Janus::RenderTargetDesc{0, 100});
    REQUIRE_FALSE(invalidCreate);
    REQUIRE(
        invalidCreate.GetError().code
        == Janus::ErrorCode::InvalidArgument);

    const auto target =
        renderer->CreateRenderTarget(
            Janus::RenderTargetDesc{640, 480});
    REQUIRE(target);

    Janus::RenderFrameDesc frame;
    frame.viewport = Janus::Viewport{640, 480};
    frame.target = target.Value();

    REQUIRE(renderer->BeginFrame(frame));

    const auto nested = renderer->BeginFrame(frame);
    REQUIRE_FALSE(nested);
    REQUIRE(
        nested.GetError().code
        == Janus::ErrorCode::InvalidState);

    const auto resizeActive =
        renderer->ResizeRenderTarget(
            target.Value(),
            800,
            600);
    REQUIRE_FALSE(resizeActive);
    REQUIRE(
        resizeActive.GetError().code
        == Janus::ErrorCode::InvalidState);

    const auto destroyActive =
        renderer->DestroyRenderTarget(target.Value());
    REQUIRE_FALSE(destroyActive);
    REQUIRE(
        destroyActive.GetError().code
        == Janus::ErrorCode::InvalidState);

    REQUIRE(renderer->EndFrame());

    REQUIRE(renderer->DestroyRenderTarget(target.Value()));

    const auto missing =
        renderer->GetRenderTargetColorTexture(target.Value());
    REQUIRE_FALSE(missing);
    REQUIRE(
        missing.GetError().code
        == Janus::ErrorCode::InvalidArgument);
}

TEST_CASE(
    "Renderer2D destroys owned render target attachments",
    "[renderer][renderer2d][render-target][v0.6]")
{
    Janus::Test::FakeRenderDevice device;

    {
        auto renderer =
            Janus::Detail::Renderer2DTestAccess::Create(device);

        const auto target =
            renderer->CreateRenderTarget(
                Janus::RenderTargetDesc{256, 256});
        REQUIRE(target);
    }

    REQUIRE(device.destroyedFramebuffers.size() == 1);
    REQUIRE(device.destroyedTextures.size() == 1);
    REQUIRE(device.destructionOrder.size() == 2);
    REQUIRE(device.destructionOrder[0] == 'F');
    REQUIRE(device.destructionOrder[1] == 'T');
}
