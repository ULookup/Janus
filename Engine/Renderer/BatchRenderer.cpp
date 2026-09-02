#include "Renderer/BatchRenderer.h"

#include <cmath>
#include <cstring>

namespace Janus
{

namespace
{

constexpr u32 Stride = sizeof(f32) * 8;

void AppendVertex(
    std::vector<f32>& vertices,
    Vector2 position,
    Vector2 uv,
    Color color)
{
    vertices.push_back(position.x);
    vertices.push_back(position.y);
    vertices.push_back(uv.x);
    vertices.push_back(uv.y);
    vertices.push_back(color.r);
    vertices.push_back(color.g);
    vertices.push_back(color.b);
    vertices.push_back(color.a);
}

} // namespace

BatchRenderer::BatchRenderer(RenderDevice& device)
    : m_Device(device)
{
}

Result<void> BatchRenderer::Flush(
    const std::vector<Batch>& batches,
    RendererStatistics& statistics)
{
    for (const Batch& batch : batches)
    {
        std::vector<f32> vertices;
        std::vector<u32> indices;

        vertices.reserve(batch.sprites.size() * 4 * 8);
        indices.reserve(batch.sprites.size() * 6);

        u32 baseIndex = 0;

        for (const Sprite& sprite : batch.sprites)
        {
            const f32 halfWidth = sprite.size.x * 0.5f;
            const f32 halfHeight = sprite.size.y * 0.5f;
            const f32 cosine = std::cos(sprite.rotationRadians);
            const f32 sine = std::sin(sprite.rotationRadians);

            const auto rotate = [&](f32 x, f32 y)
            {
                return Vector2{
                    x * cosine - y * sine,
                    x * sine + y * cosine};
            };

            const Vector2 corners[]{
                rotate(-halfWidth, -halfHeight),
                rotate(halfWidth, -halfHeight),
                rotate(halfWidth, halfHeight),
                rotate(-halfWidth, halfHeight)};

            const Vector2 uv[]{
                {sprite.uv.min.x, sprite.uv.min.y},
                {sprite.uv.max.x, sprite.uv.min.y},
                {sprite.uv.max.x, sprite.uv.max.y},
                {sprite.uv.min.x, sprite.uv.max.y}};

            for (u32 index = 0; index < 4; ++index)
            {
                AppendVertex(
                    vertices,
                    Vector2{
                        corners[index].x + sprite.position.x,
                        corners[index].y + sprite.position.y},
                    uv[index],
                    sprite.color);
            }

            const u32 offset = baseIndex;
            const u32 quadIndices[]{
                offset,
                offset + 1,
                offset + 2,
                offset + 2,
                offset + 3,
                offset};

            indices.insert(
                indices.end(),
                std::begin(quadIndices),
                std::end(quadIndices));

            baseIndex += 4;
        }

        const BufferDesc vertexDesc{
            vertices.data(),
            vertices.size() * sizeof(f32)};

        const auto vertexBuffer =
            m_Device.CreateVertexBuffer(vertexDesc);

        if (!vertexBuffer)
        {
            return Result<void>::Failure(vertexBuffer.GetError());
        }

        const BufferDesc indexDesc{
            indices.data(),
            indices.size() * sizeof(u32)};

        const auto indexBuffer =
            m_Device.CreateIndexBuffer(indexDesc);

        if (!indexBuffer)
        {
            m_Device.DestroyVertexBuffer(vertexBuffer.Value());
            return Result<void>::Failure(indexBuffer.GetError());
        }

        VertexLayout layout;
        layout.stride = Stride;
        layout.attributes = {
            {0, VertexAttributeType::Float2, 0},
            {1, VertexAttributeType::Float2, sizeof(f32) * 2},
            {2, VertexAttributeType::Float4, sizeof(f32) * 4}};

        const auto vertexArray =
            m_Device.CreateVertexArray(layout);

        if (!vertexArray)
        {
            m_Device.DestroyIndexBuffer(indexBuffer.Value());
            m_Device.DestroyVertexBuffer(vertexBuffer.Value());
            return Result<void>::Failure(vertexArray.GetError());
        }

        DrawCommand command;
        command.vertexArray = vertexArray.Value();
        command.indexBuffer = indexBuffer.Value();
        command.indexCount =
            static_cast<u32>(indices.size());
        command.texture = batch.texture;

        m_Device.DrawIndexed(command);

        m_Device.DestroyVertexArray(vertexArray.Value());
        m_Device.DestroyIndexBuffer(indexBuffer.Value());
        m_Device.DestroyVertexBuffer(vertexBuffer.Value());

        ++statistics.batchCount;
        ++statistics.drawCallCount;
        ++statistics.textureBindCount;
        statistics.spriteCount +=
            static_cast<u32>(batch.sprites.size());
        statistics.vertexCount +=
            static_cast<u32>(vertices.size() / 8);
        statistics.indexCount +=
            static_cast<u32>(indices.size());
    }

    return Result<void>::Success();
}

} // namespace Janus
