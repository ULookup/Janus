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

struct CreatedFramebufferRecord
{
    FramebufferHandle handle;
    FramebufferDesc desc;
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
        if (failNextTextureCreate)
        {
            failNextTextureCreate = false;
            return Result<TextureHandle>::Failure(
                ErrorCode::TextureCreateFailed,
                "Fake texture creation failure.");
        }

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
        destructionOrder.push_back('T');
    }

    Result<FramebufferHandle> CreateFramebuffer(
        const FramebufferDesc& desc) override
    {
        if (failNextFramebufferCreate)
        {
            failNextFramebufferCreate = false;
            return Result<FramebufferHandle>::Failure(
                ErrorCode::FramebufferCreateFailed,
                "Fake framebuffer creation failure.");
        }

        const FramebufferHandle handle{Next()};
        createdFramebuffers.push_back(
            CreatedFramebufferRecord{handle, desc});
        return Result<FramebufferHandle>::Success(handle);
    }

    void DestroyFramebuffer(FramebufferHandle handle) override
    {
        destroyedFramebuffers.push_back(handle);
        destructionOrder.push_back('F');
    }

    Result<void> BindFramebuffer(
        FramebufferHandle handle) override
    {
        if (failNextFramebufferBind)
        {
            failNextFramebufferBind = false;
            return Result<void>::Failure(
                ErrorCode::InvalidState,
                "Fake framebuffer bind failure.");
        }

        boundFramebuffers.push_back(handle);
        return Result<void>::Success();
    }

    void BindDefaultFramebuffer() override
    {
        ++defaultFramebufferBindCount;
    }

    Result<TexturePresentationHandle> GetTexturePresentationHandle(
        TextureHandle handle) const override
    {
        if (handle.value == 0)
        {
            return Result<TexturePresentationHandle>::Failure(
                ErrorCode::InvalidArgument,
                "Fake cannot present an invalid texture.");
        }

        return Result<TexturePresentationHandle>::Success(
            TexturePresentationHandle{
                static_cast<usize>(handle.value)});
    }

    void SetViewport(Viewport viewport) override
    {
        lastViewport = viewport;
        viewportHistory.push_back(viewport);
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

    bool failNextTextureCreate = false;
    bool failNextFramebufferCreate = false;
    bool failNextFramebufferBind = false;

    std::vector<DrawCommand> drawCommands;
    std::vector<CreatedTextureRecord> createdTextures;
    std::vector<TextureHandle> destroyedTextures;
    std::vector<CreatedFramebufferRecord> createdFramebuffers;
    std::vector<FramebufferHandle> destroyedFramebuffers;
    std::vector<FramebufferHandle> boundFramebuffers;
    std::vector<Viewport> viewportHistory;
    std::vector<char> destructionOrder;

    u32 defaultFramebufferBindCount = 0;
    Viewport lastViewport;
    Color lastClearColor;

private:
    u32 nextHandle = 0;
};

} // namespace Janus::Test
