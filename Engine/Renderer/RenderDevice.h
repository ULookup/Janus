#pragma once

#include "Core/Error/Result.h"
#include "Core/Math/Mat4.h"
#include "Renderer/RendererTypes.h"

namespace Janus
{

struct DrawCommand
{
    VertexArrayHandle vertexArray;
    IndexBufferHandle indexBuffer;
    u32 indexCount = 0;
    TextureHandle texture;
};

class RenderDevice
{
public:
    virtual ~RenderDevice() = default;

    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    virtual Result<VertexBufferHandle> CreateVertexBuffer(
        const BufferDesc& desc) = 0;
    virtual void DestroyVertexBuffer(VertexBufferHandle handle) = 0;

    virtual Result<IndexBufferHandle> CreateIndexBuffer(
        const BufferDesc& desc) = 0;
    virtual void DestroyIndexBuffer(IndexBufferHandle handle) = 0;

    virtual Result<VertexArrayHandle> CreateVertexArray(
        const VertexLayout& layout,
        VertexBufferHandle vertexBuffer) = 0;
    virtual void DestroyVertexArray(VertexArrayHandle handle) = 0;

    virtual Result<ShaderHandle> CreateShader(
        const ShaderDesc& desc) = 0;
    virtual void DestroyShader(ShaderHandle handle) = 0;

    virtual Result<TextureHandle> CreateTexture(
        const TextureDesc& desc) = 0;
    virtual void DestroyTexture(TextureHandle handle) = 0;

    virtual Result<FramebufferHandle> CreateFramebuffer(
        const FramebufferDesc& desc) = 0;
    virtual void DestroyFramebuffer(FramebufferHandle handle) = 0;
    virtual Result<void> BindFramebuffer(FramebufferHandle handle) = 0;
    virtual void BindDefaultFramebuffer() = 0;

    virtual void SetViewport(Viewport viewport) = 0;
    virtual void SetViewProjection(const Mat4& matrix) = 0;
    virtual void UseShader(ShaderHandle handle) = 0;
    virtual void Clear(Color color) = 0;
    virtual void DrawIndexed(const DrawCommand& command) = 0;

protected:
    RenderDevice() = default;
};

} // namespace Janus
