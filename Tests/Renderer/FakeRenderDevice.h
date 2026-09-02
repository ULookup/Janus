#pragma once

#include "Renderer/RenderDevice.h"

#include <vector>

namespace Janus::Test
{

class FakeRenderDevice final : public RenderDevice
{
public:
    Result<VertexBufferHandle> CreateVertexBuffer(
        const BufferDesc&) override
    {
        return Result<VertexBufferHandle>::Success(
            VertexBufferHandle{Next()});
    }

    void DestroyVertexBuffer(VertexBufferHandle) override
    {
    }

    Result<IndexBufferHandle> CreateIndexBuffer(
        const BufferDesc&) override
    {
        return Result<IndexBufferHandle>::Success(
            IndexBufferHandle{Next()});
    }

    void DestroyIndexBuffer(IndexBufferHandle) override
    {
    }

    Result<VertexArrayHandle> CreateVertexArray(
        const VertexLayout&) override
    {
        return Result<VertexArrayHandle>::Success(
            VertexArrayHandle{Next()});
    }

    void DestroyVertexArray(VertexArrayHandle) override
    {
    }

    Result<ShaderHandle> CreateShader(
        const ShaderDesc&) override
    {
        return Result<ShaderHandle>::Success(
            ShaderHandle{Next()});
    }

    void DestroyShader(ShaderHandle) override
    {
    }

    Result<TextureHandle> CreateTexture(
        const TextureDesc&) override
    {
        return Result<TextureHandle>::Success(
            TextureHandle{Next()});
    }

    void DestroyTexture(TextureHandle) override
    {
    }

    Result<FramebufferHandle> CreateFramebuffer(
        const FramebufferDesc&) override
    {
        return Result<FramebufferHandle>::Success(
            FramebufferHandle{Next()});
    }

    void DestroyFramebuffer(FramebufferHandle) override
    {
    }

    void SetViewport(Viewport viewport) override
    {
        lastViewport = viewport;
    }

    void Clear(Color color) override
    {
        lastClearColor = color;
    }

    void DrawIndexed(const DrawCommand& command) override
    {
        drawCommands.push_back(command);
    }

    u32 Next()
    {
        return ++nextHandle;
    }

    std::vector<DrawCommand> drawCommands;
    Viewport lastViewport;
    Color lastClearColor;

private:
    u32 nextHandle = 0;
};

} // namespace Janus::Test
