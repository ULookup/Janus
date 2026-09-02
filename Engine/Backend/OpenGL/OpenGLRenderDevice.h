#pragma once

#include "Core/Math/Mat4.h"
#include "Renderer/RenderDevice.h"

#include <memory>
#include <unordered_map>

namespace Janus
{

class OpenGLRenderDevice final : public RenderDevice
{
public:
    [[nodiscard]]
    static Result<std::unique_ptr<OpenGLRenderDevice>> Create();

    ~OpenGLRenderDevice() override;

    Result<VertexBufferHandle> CreateVertexBuffer(
        const BufferDesc& desc) override;
    void DestroyVertexBuffer(VertexBufferHandle handle) override;

    Result<IndexBufferHandle> CreateIndexBuffer(
        const BufferDesc& desc) override;
    void DestroyIndexBuffer(IndexBufferHandle handle) override;

    Result<VertexArrayHandle> CreateVertexArray(
        const VertexLayout& layout,
        VertexBufferHandle vertexBuffer) override;
    void DestroyVertexArray(VertexArrayHandle handle) override;

    Result<ShaderHandle> CreateShader(
        const ShaderDesc& desc) override;
    void DestroyShader(ShaderHandle handle) override;

    Result<TextureHandle> CreateTexture(
        const TextureDesc& desc) override;
    void DestroyTexture(TextureHandle handle) override;

    Result<FramebufferHandle> CreateFramebuffer(
        const FramebufferDesc& desc) override;
    void DestroyFramebuffer(FramebufferHandle handle) override;

    void SetViewport(Viewport viewport) override;
    void SetViewProjection(const Mat4& matrix) override;
    void UseShader(ShaderHandle handle) override;
    void Clear(Color color) override;
    void DrawIndexed(const DrawCommand& command) override;

private:
    OpenGLRenderDevice() = default;

    u32 m_NextHandle = 1;
    Viewport m_Viewport;
    Mat4 m_ViewProjection;
    ShaderHandle m_CurrentShader;

    std::unordered_map<u32, u32> m_VertexBuffers;
    std::unordered_map<u32, u32> m_IndexBuffers;
    std::unordered_map<u32, u32> m_VertexArrays;
    std::unordered_map<u32, u32> m_Shaders;
    std::unordered_map<u32, u32> m_Textures;
    std::unordered_map<u32, u32> m_Framebuffers;
};

} // namespace Janus
