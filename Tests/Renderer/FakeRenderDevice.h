#pragma once

#include "Renderer/RenderDevice.h"

#include <utility>
#include <vector>

namespace Janus::Test
{

struct CreatedTextureRecord
{
    TextureHandle handle;
    u32 width = 0;
    u32 height = 0;
    usize dataSize = 0;
    std::vector<u8> pixels;
};

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
        const VertexLayout&,
        VertexBufferHandle) override
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
        const TextureDesc& desc) override
    {
        const TextureHandle handle{Next()};

        CreatedTextureRecord record;
        record.handle = handle;
        record.width = desc.width;
        record.height = desc.height;
        record.dataSize = desc.dataSize;

        if (desc.data != nullptr && desc.dataSize != 0)
        {
            const auto* begin = static_cast<const u8*>(desc.data);
            record.pixels.assign(begin, begin + desc.dataSize);
        }

        createdTextures.push_back(std::move(record));
        return Result<TextureHandle>::Success(handle);
    }

    void DestroyTexture(TextureHandle handle) override
    {
        destroyedTextures.push_back(handle);
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

    void SetViewProjection(const Mat4&) override
    {
    }

    void UseShader(ShaderHandle) override
    {
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
    std::vector<CreatedTextureRecord> createdTextures;
    std::vector<TextureHandle> destroyedTextures;
    Viewport lastViewport;
    Color lastClearColor;

private:
    u32 nextHandle = 0;
};

} // namespace Janus::Test
